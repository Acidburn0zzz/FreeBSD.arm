/*-
 * Copyright (C) 2012-2014 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/nvme/nvme_qpair.c 293326 2016-01-07 16:11:31Z jimharris $");

#include <sys/param.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>

#include "nvme_private.h"

static void	_nvme_qpair_submit_request(struct nvme_qpair *qpair,
					   struct nvme_request *req);

struct nvme_opcode_string {

	uint16_t	opc;
	const char *	str;
};

static struct nvme_opcode_string admin_opcode[] = {
	{ NVME_OPC_DELETE_IO_SQ, "DELETE IO SQ" },
	{ NVME_OPC_CREATE_IO_SQ, "CREATE IO SQ" },
	{ NVME_OPC_GET_LOG_PAGE, "GET LOG PAGE" },
	{ NVME_OPC_DELETE_IO_CQ, "DELETE IO CQ" },
	{ NVME_OPC_CREATE_IO_CQ, "CREATE IO CQ" },
	{ NVME_OPC_IDENTIFY, "IDENTIFY" },
	{ NVME_OPC_ABORT, "ABORT" },
	{ NVME_OPC_SET_FEATURES, "SET FEATURES" },
	{ NVME_OPC_GET_FEATURES, "GET FEATURES" },
	{ NVME_OPC_ASYNC_EVENT_REQUEST, "ASYNC EVENT REQUEST" },
	{ NVME_OPC_FIRMWARE_ACTIVATE, "FIRMWARE ACTIVATE" },
	{ NVME_OPC_FIRMWARE_IMAGE_DOWNLOAD, "FIRMWARE IMAGE DOWNLOAD" },
	{ NVME_OPC_FORMAT_NVM, "FORMAT NVM" },
	{ NVME_OPC_SECURITY_SEND, "SECURITY SEND" },
	{ NVME_OPC_SECURITY_RECEIVE, "SECURITY RECEIVE" },
	{ 0xFFFF, "ADMIN COMMAND" }
};

static struct nvme_opcode_string io_opcode[] = {
	{ NVME_OPC_FLUSH, "FLUSH" },
	{ NVME_OPC_WRITE, "WRITE" },
	{ NVME_OPC_READ, "READ" },
	{ NVME_OPC_WRITE_UNCORRECTABLE, "WRITE UNCORRECTABLE" },
	{ NVME_OPC_COMPARE, "COMPARE" },
	{ NVME_OPC_DATASET_MANAGEMENT, "DATASET MANAGEMENT" },
	{ 0xFFFF, "IO COMMAND" }
};

static const char *
get_admin_opcode_string(uint16_t opc)
{
	struct nvme_opcode_string *entry;

	entry = admin_opcode;

	while (entry->opc != 0xFFFF) {
		if (entry->opc == opc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}

static const char *
get_io_opcode_string(uint16_t opc)
{
	struct nvme_opcode_string *entry;

	entry = io_opcode;

	while (entry->opc != 0xFFFF) {
		if (entry->opc == opc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}


static void
nvme_admin_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{

	nvme_printf(qpair->ctrlr, "%s (%02x) sqid:%d cid:%d nsid:%x "
	    "cdw10:%08x cdw11:%08x\n",
	    get_admin_opcode_string(cmd->opc), cmd->opc, qpair->id, cmd->cid,
	    cmd->nsid, cmd->cdw10, cmd->cdw11);
}

static void
nvme_io_qpair_print_command(struct nvme_qpair *qpair,
    struct nvme_command *cmd)
{

	switch (cmd->opc) {
	case NVME_OPC_WRITE:
	case NVME_OPC_READ:
	case NVME_OPC_WRITE_UNCORRECTABLE:
	case NVME_OPC_COMPARE:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d "
		    "lba:%llu len:%d\n",
		    get_io_opcode_string(cmd->opc), qpair->id, cmd->cid,
		    cmd->nsid,
		    ((unsigned long long)cmd->cdw11 << 32) + cmd->cdw10,
		    (cmd->cdw12 & 0xFFFF) + 1);
		break;
	case NVME_OPC_FLUSH:
	case NVME_OPC_DATASET_MANAGEMENT:
		nvme_printf(qpair->ctrlr, "%s sqid:%d cid:%d nsid:%d\n",
		    get_io_opcode_string(cmd->opc), qpair->id, cmd->cid,
		    cmd->nsid);
		break;
	default:
		nvme_printf(qpair->ctrlr, "%s (%02x) sqid:%d cid:%d nsid:%d\n",
		    get_io_opcode_string(cmd->opc), cmd->opc, qpair->id,
		    cmd->cid, cmd->nsid);
		break;
	}
}

static void
nvme_qpair_print_command(struct nvme_qpair *qpair, struct nvme_command *cmd)
{
	if (qpair->id == 0)
		nvme_admin_qpair_print_command(qpair, cmd);
	else
		nvme_io_qpair_print_command(qpair, cmd);
}

struct nvme_status_string {

	uint16_t	sc;
	const char *	str;
};

static struct nvme_status_string generic_status[] = {
	{ NVME_SC_SUCCESS, "SUCCESS" },
	{ NVME_SC_INVALID_OPCODE, "INVALID OPCODE" },
	{ NVME_SC_INVALID_FIELD, "INVALID_FIELD" },
	{ NVME_SC_COMMAND_ID_CONFLICT, "COMMAND ID CONFLICT" },
	{ NVME_SC_DATA_TRANSFER_ERROR, "DATA TRANSFER ERROR" },
	{ NVME_SC_ABORTED_POWER_LOSS, "ABORTED - POWER LOSS" },
	{ NVME_SC_INTERNAL_DEVICE_ERROR, "INTERNAL DEVICE ERROR" },
	{ NVME_SC_ABORTED_BY_REQUEST, "ABORTED - BY REQUEST" },
	{ NVME_SC_ABORTED_SQ_DELETION, "ABORTED - SQ DELETION" },
	{ NVME_SC_ABORTED_FAILED_FUSED, "ABORTED - FAILED FUSED" },
	{ NVME_SC_ABORTED_MISSING_FUSED, "ABORTED - MISSING FUSED" },
	{ NVME_SC_INVALID_NAMESPACE_OR_FORMAT, "INVALID NAMESPACE OR FORMAT" },
	{ NVME_SC_COMMAND_SEQUENCE_ERROR, "COMMAND SEQUENCE ERROR" },
	{ NVME_SC_LBA_OUT_OF_RANGE, "LBA OUT OF RANGE" },
	{ NVME_SC_CAPACITY_EXCEEDED, "CAPACITY EXCEEDED" },
	{ NVME_SC_NAMESPACE_NOT_READY, "NAMESPACE NOT READY" },
	{ 0xFFFF, "GENERIC" }
};

static struct nvme_status_string command_specific_status[] = {
	{ NVME_SC_COMPLETION_QUEUE_INVALID, "INVALID COMPLETION QUEUE" },
	{ NVME_SC_INVALID_QUEUE_IDENTIFIER, "INVALID QUEUE IDENTIFIER" },
	{ NVME_SC_MAXIMUM_QUEUE_SIZE_EXCEEDED, "MAX QUEUE SIZE EXCEEDED" },
	{ NVME_SC_ABORT_COMMAND_LIMIT_EXCEEDED, "ABORT CMD LIMIT EXCEEDED" },
	{ NVME_SC_ASYNC_EVENT_REQUEST_LIMIT_EXCEEDED, "ASYNC LIMIT EXCEEDED" },
	{ NVME_SC_INVALID_FIRMWARE_SLOT, "INVALID FIRMWARE SLOT" },
	{ NVME_SC_INVALID_FIRMWARE_IMAGE, "INVALID FIRMWARE IMAGE" },
	{ NVME_SC_INVALID_INTERRUPT_VECTOR, "INVALID INTERRUPT VECTOR" },
	{ NVME_SC_INVALID_LOG_PAGE, "INVALID LOG PAGE" },
	{ NVME_SC_INVALID_FORMAT, "INVALID FORMAT" },
	{ NVME_SC_FIRMWARE_REQUIRES_RESET, "FIRMWARE REQUIRES RESET" },
	{ NVME_SC_CONFLICTING_ATTRIBUTES, "CONFLICTING ATTRIBUTES" },
	{ NVME_SC_INVALID_PROTECTION_INFO, "INVALID PROTECTION INFO" },
	{ NVME_SC_ATTEMPTED_WRITE_TO_RO_PAGE, "WRITE TO RO PAGE" },
	{ 0xFFFF, "COMMAND SPECIFIC" }
};

static struct nvme_status_string media_error_status[] = {
	{ NVME_SC_WRITE_FAULTS, "WRITE FAULTS" },
	{ NVME_SC_UNRECOVERED_READ_ERROR, "UNRECOVERED READ ERROR" },
	{ NVME_SC_GUARD_CHECK_ERROR, "GUARD CHECK ERROR" },
	{ NVME_SC_APPLICATION_TAG_CHECK_ERROR, "APPLICATION TAG CHECK ERROR" },
	{ NVME_SC_REFERENCE_TAG_CHECK_ERROR, "REFERENCE TAG CHECK ERROR" },
	{ NVME_SC_COMPARE_FAILURE, "COMPARE FAILURE" },
	{ NVME_SC_ACCESS_DENIED, "ACCESS DENIED" },
	{ 0xFFFF, "MEDIA ERROR" }
};

static const char *
get_status_string(uint16_t sct, uint16_t sc)
{
	struct nvme_status_string *entry;

	switch (sct) {
	case NVME_SCT_GENERIC:
		entry = generic_status;
		break;
	case NVME_SCT_COMMAND_SPECIFIC:
		entry = command_specific_status;
		break;
	case NVME_SCT_MEDIA_ERROR:
		entry = media_error_status;
		break;
	case NVME_SCT_VENDOR_SPECIFIC:
		return ("VENDOR SPECIFIC");
	default:
		return ("RESERVED");
	}

	while (entry->sc != 0xFFFF) {
		if (entry->sc == sc)
			return (entry->str);
		entry++;
	}
	return (entry->str);
}

static void
nvme_qpair_print_completion(struct nvme_qpair *qpair, 
    struct nvme_completion *cpl)
{
	nvme_printf(qpair->ctrlr, "%s (%02x/%02x) sqid:%d cid:%d cdw0:%x\n",
	    get_status_string(cpl->status.sct, cpl->status.sc),
	    cpl->status.sct, cpl->status.sc, cpl->sqid, cpl->cid, cpl->cdw0);
}

static boolean_t
nvme_completion_is_retry(const struct nvme_completion *cpl)
{
	/*
	 * TODO: spec is not clear how commands that are aborted due
	 *  to TLER will be marked.  So for now, it seems
	 *  NAMESPACE_NOT_READY is the only case where we should
	 *  look at the DNR bit.
	 */
	switch (cpl->status.sct) {
	case NVME_SCT_GENERIC:
		switch (cpl->status.sc) {
		case NVME_SC_ABORTED_BY_REQUEST:
		case NVME_SC_NAMESPACE_NOT_READY:
			if (cpl->status.dnr)
				return (0);
			else
				return (1);
		case NVME_SC_INVALID_OPCODE:
		case NVME_SC_INVALID_FIELD:
		case NVME_SC_COMMAND_ID_CONFLICT:
		case NVME_SC_DATA_TRANSFER_ERROR:
		case NVME_SC_ABORTED_POWER_LOSS:
		case NVME_SC_INTERNAL_DEVICE_ERROR:
		case NVME_SC_ABORTED_SQ_DELETION:
		case NVME_SC_ABORTED_FAILED_FUSED:
		case NVME_SC_ABORTED_MISSING_FUSED:
		case NVME_SC_INVALID_NAMESPACE_OR_FORMAT:
		case NVME_SC_COMMAND_SEQUENCE_ERROR:
		case NVME_SC_LBA_OUT_OF_RANGE:
		case NVME_SC_CAPACITY_EXCEEDED:
		default:
			return (0);
		}
	case NVME_SCT_COMMAND_SPECIFIC:
	case NVME_SCT_MEDIA_ERROR:
	case NVME_SCT_VENDOR_SPECIFIC:
	default:
		return (0);
	}
}

static void
nvme_qpair_construct_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr,
    uint16_t cid)
{

	bus_dmamap_create(qpair->dma_tag_payload, 0, &tr->payload_dma_map);
	bus_dmamap_create(qpair->dma_tag, 0, &tr->prp_dma_map);

	bus_dmamap_load(qpair->dma_tag, tr->prp_dma_map, tr->prp,
	    sizeof(tr->prp), nvme_single_map, &tr->prp_bus_addr, 0);

	callout_init(&tr->timer, 1);
	tr->cid = cid;
	tr->qpair = qpair;
}

static void
nvme_qpair_complete_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr,
    struct nvme_completion *cpl, boolean_t print_on_error)
{
	struct nvme_request	*req;
	boolean_t		retry, error;

	req = tr->req;
	error = nvme_completion_is_error(cpl);
	retry = error && nvme_completion_is_retry(cpl) &&
	   req->retries < nvme_retry_count;

	if (error && print_on_error) {
		nvme_qpair_print_command(qpair, &req->cmd);
		nvme_qpair_print_completion(qpair, cpl);
	}

	qpair->act_tr[cpl->cid] = NULL;

	KASSERT(cpl->cid == req->cmd.cid, ("cpl cid does not match cmd cid\n"));

	if (req->cb_fn && !retry)
		req->cb_fn(req->cb_arg, cpl);

	mtx_lock(&qpair->lock);
	callout_stop(&tr->timer);

	if (retry) {
		req->retries++;
		nvme_qpair_submit_tracker(qpair, tr);
	} else {
		if (req->type != NVME_REQUEST_NULL)
			bus_dmamap_unload(qpair->dma_tag_payload,
			    tr->payload_dma_map);

		nvme_free_request(req);
		tr->req = NULL;

		TAILQ_REMOVE(&qpair->outstanding_tr, tr, tailq);
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);

		/*
		 * If the controller is in the middle of resetting, don't
		 *  try to submit queued requests here - let the reset logic
		 *  handle that instead.
		 */
		if (!STAILQ_EMPTY(&qpair->queued_req) &&
		    !qpair->ctrlr->is_resetting) {
			req = STAILQ_FIRST(&qpair->queued_req);
			STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
			_nvme_qpair_submit_request(qpair, req);
		}
	}

	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_manual_complete_tracker(struct nvme_qpair *qpair,
    struct nvme_tracker *tr, uint32_t sct, uint32_t sc, uint32_t dnr,
    boolean_t print_on_error)
{
	struct nvme_completion	cpl;

	memset(&cpl, 0, sizeof(cpl));
	cpl.sqid = qpair->id;
	cpl.cid = tr->cid;
	cpl.status.sct = sct;
	cpl.status.sc = sc;
	cpl.status.dnr = dnr;
	nvme_qpair_complete_tracker(qpair, tr, &cpl, print_on_error);
}

void
nvme_qpair_manual_complete_request(struct nvme_qpair *qpair,
    struct nvme_request *req, uint32_t sct, uint32_t sc,
    boolean_t print_on_error)
{
	struct nvme_completion	cpl;
	boolean_t		error;

	memset(&cpl, 0, sizeof(cpl));
	cpl.sqid = qpair->id;
	cpl.status.sct = sct;
	cpl.status.sc = sc;

	error = nvme_completion_is_error(&cpl);

	if (error && print_on_error) {
		nvme_qpair_print_command(qpair, &req->cmd);
		nvme_qpair_print_completion(qpair, &cpl);
	}

	if (req->cb_fn)
		req->cb_fn(req->cb_arg, &cpl);

	nvme_free_request(req);
}

void
nvme_qpair_process_completions(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;
	struct nvme_completion	*cpl;

	qpair->num_intr_handler_calls++;

	if (!qpair->is_enabled)
		/*
		 * qpair is not enabled, likely because a controller reset is
		 *  is in progress.  Ignore the interrupt - any I/O that was
		 *  associated with this interrupt will get retried when the
		 *  reset is complete.
		 */
		return;

	while (1) {
		cpl = &qpair->cpl[qpair->cq_head];

		if (cpl->status.p != qpair->phase)
			break;

		tr = qpair->act_tr[cpl->cid];

		if (tr != NULL) {
			nvme_qpair_complete_tracker(qpair, tr, cpl, TRUE);
			qpair->sq_head = cpl->sqhd;
		} else {
			nvme_printf(qpair->ctrlr, 
			    "cpl does not map to outstanding cmd\n");
			nvme_dump_completion(cpl);
			KASSERT(0, ("received completion for unknown cmd\n"));
		}

		if (++qpair->cq_head == qpair->num_entries) {
			qpair->cq_head = 0;
			qpair->phase = !qpair->phase;
		}

		nvme_mmio_write_4(qpair->ctrlr, doorbell[qpair->id].cq_hdbl,
		    qpair->cq_head);
	}
}

static void
nvme_qpair_msix_handler(void *arg)
{
	struct nvme_qpair *qpair = arg;

	nvme_qpair_process_completions(qpair);
}

void
nvme_qpair_construct(struct nvme_qpair *qpair, uint32_t id,
    uint16_t vector, uint32_t num_entries, uint32_t num_trackers,
    struct nvme_controller *ctrlr)
{
	struct nvme_tracker	*tr;
	uint32_t		i;
	int			err;

	qpair->id = id;
	qpair->vector = vector;
	qpair->num_entries = num_entries;
	qpair->num_trackers = num_trackers;
	qpair->ctrlr = ctrlr;

	if (ctrlr->msix_enabled) {

		/*
		 * MSI-X vector resource IDs start at 1, so we add one to
		 *  the queue's vector to get the corresponding rid to use.
		 */
		qpair->rid = vector + 1;

		qpair->res = bus_alloc_resource_any(ctrlr->dev, SYS_RES_IRQ,
		    &qpair->rid, RF_ACTIVE);
		bus_setup_intr(ctrlr->dev, qpair->res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL,
		    nvme_qpair_msix_handler, qpair, &qpair->tag);
	}

	mtx_init(&qpair->lock, "nvme qpair lock", NULL, MTX_DEF);

	/* Note: NVMe PRP format is restricted to 4-byte alignment. */
	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    4, PAGE_SIZE, BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR, NULL, NULL, NVME_MAX_XFER_SIZE,
	    (NVME_MAX_XFER_SIZE/PAGE_SIZE)+1, PAGE_SIZE, 0,
	    NULL, NULL, &qpair->dma_tag_payload);
	if (err != 0)
		nvme_printf(ctrlr, "payload tag create failed %d\n", err);

	err = bus_dma_tag_create(bus_get_dma_tag(ctrlr->dev),
	    4, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    BUS_SPACE_MAXSIZE, 1, BUS_SPACE_MAXSIZE, 0,
	    NULL, NULL, &qpair->dma_tag);
	if (err != 0)
		nvme_printf(ctrlr, "tag create failed %d\n", err);

	qpair->num_cmds = 0;
	qpair->num_intr_handler_calls = 0;

	qpair->cmd = contigmalloc(qpair->num_entries *
	    sizeof(struct nvme_command), M_NVME, M_ZERO,
	    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);
	qpair->cpl = contigmalloc(qpair->num_entries *
	    sizeof(struct nvme_completion), M_NVME, M_ZERO,
	    0, BUS_SPACE_MAXADDR, PAGE_SIZE, 0);

	err = bus_dmamap_create(qpair->dma_tag, 0, &qpair->cmd_dma_map);
	if (err != 0)
		nvme_printf(ctrlr, "cmd_dma_map create failed %d\n", err);

	err = bus_dmamap_create(qpair->dma_tag, 0, &qpair->cpl_dma_map);
	if (err != 0)
		nvme_printf(ctrlr, "cpl_dma_map create failed %d\n", err);

	bus_dmamap_load(qpair->dma_tag, qpair->cmd_dma_map,
	    qpair->cmd, qpair->num_entries * sizeof(struct nvme_command),
	    nvme_single_map, &qpair->cmd_bus_addr, 0);
	bus_dmamap_load(qpair->dma_tag, qpair->cpl_dma_map,
	    qpair->cpl, qpair->num_entries * sizeof(struct nvme_completion),
	    nvme_single_map, &qpair->cpl_bus_addr, 0);

	qpair->sq_tdbl_off = nvme_mmio_offsetof(doorbell[id].sq_tdbl);
	qpair->cq_hdbl_off = nvme_mmio_offsetof(doorbell[id].cq_hdbl);

	TAILQ_INIT(&qpair->free_tr);
	TAILQ_INIT(&qpair->outstanding_tr);
	STAILQ_INIT(&qpair->queued_req);

	for (i = 0; i < qpair->num_trackers; i++) {
		tr = malloc(sizeof(*tr), M_NVME, M_ZERO | M_WAITOK);
		nvme_qpair_construct_tracker(qpair, tr, i);
		TAILQ_INSERT_HEAD(&qpair->free_tr, tr, tailq);
	}

	qpair->act_tr = malloc(sizeof(struct nvme_tracker *) * qpair->num_entries,
	    M_NVME, M_ZERO | M_WAITOK);
}

static void
nvme_qpair_destroy(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	if (qpair->tag)
		bus_teardown_intr(qpair->ctrlr->dev, qpair->res, qpair->tag);

	if (qpair->res)
		bus_release_resource(qpair->ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(qpair->res), qpair->res);

	if (qpair->cmd) {
		bus_dmamap_unload(qpair->dma_tag, qpair->cmd_dma_map);
		bus_dmamap_destroy(qpair->dma_tag, qpair->cmd_dma_map);
		contigfree(qpair->cmd,
		    qpair->num_entries * sizeof(struct nvme_command), M_NVME);
	}

	if (qpair->cpl) {
		bus_dmamap_unload(qpair->dma_tag, qpair->cpl_dma_map);
		bus_dmamap_destroy(qpair->dma_tag, qpair->cpl_dma_map);
		contigfree(qpair->cpl,
		    qpair->num_entries * sizeof(struct nvme_completion),
		    M_NVME);
	}

	if (qpair->dma_tag)
		bus_dma_tag_destroy(qpair->dma_tag);

	if (qpair->dma_tag_payload)
		bus_dma_tag_destroy(qpair->dma_tag_payload);

	if (qpair->act_tr)
		free(qpair->act_tr, M_NVME);

	while (!TAILQ_EMPTY(&qpair->free_tr)) {
		tr = TAILQ_FIRST(&qpair->free_tr);
		TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
		bus_dmamap_destroy(qpair->dma_tag, tr->payload_dma_map);
		bus_dmamap_destroy(qpair->dma_tag, tr->prp_dma_map);
		free(tr, M_NVME);
	}
}

static void
nvme_admin_qpair_abort_aers(struct nvme_qpair *qpair)
{
	struct nvme_tracker	*tr;

	tr = TAILQ_FIRST(&qpair->outstanding_tr);
	while (tr != NULL) {
		if (tr->req->cmd.opc == NVME_OPC_ASYNC_EVENT_REQUEST) {
			nvme_qpair_manual_complete_tracker(qpair, tr,
			    NVME_SCT_GENERIC, NVME_SC_ABORTED_SQ_DELETION, 0,
			    FALSE);
			tr = TAILQ_FIRST(&qpair->outstanding_tr);
		} else {
			tr = TAILQ_NEXT(tr, tailq);
		}
	}
}

void
nvme_admin_qpair_destroy(struct nvme_qpair *qpair)
{

	nvme_admin_qpair_abort_aers(qpair);
	nvme_qpair_destroy(qpair);
}

void
nvme_io_qpair_destroy(struct nvme_qpair *qpair)
{

	nvme_qpair_destroy(qpair);
}

static void
nvme_abort_complete(void *arg, const struct nvme_completion *status)
{
	struct nvme_tracker	*tr = arg;

	/*
	 * If cdw0 == 1, the controller was not able to abort the command
	 *  we requested.  We still need to check the active tracker array,
	 *  to cover race where I/O timed out at same time controller was
	 *  completing the I/O.
	 */
	if (status->cdw0 == 1 && tr->qpair->act_tr[tr->cid] != NULL) {
		/*
		 * An I/O has timed out, and the controller was unable to
		 *  abort it for some reason.  Construct a fake completion
		 *  status, and then complete the I/O's tracker manually.
		 */
		nvme_printf(tr->qpair->ctrlr,
		    "abort command failed, aborting command manually\n");
		nvme_qpair_manual_complete_tracker(tr->qpair, tr,
		    NVME_SCT_GENERIC, NVME_SC_ABORTED_BY_REQUEST, 0, TRUE);
	}
}

static void
nvme_timeout(void *arg)
{
	struct nvme_tracker	*tr = arg;
	struct nvme_qpair	*qpair = tr->qpair;
	struct nvme_controller	*ctrlr = qpair->ctrlr;
	union csts_register	csts;

	/* Read csts to get value of cfs - controller fatal status. */
	csts.raw = nvme_mmio_read_4(ctrlr, csts);

	if (ctrlr->enable_aborts && csts.bits.cfs == 0) {
		/*
		 * If aborts are enabled, only use them if the controller is
		 *  not reporting fatal status.
		 */
		nvme_ctrlr_cmd_abort(ctrlr, tr->cid, qpair->id,
		    nvme_abort_complete, tr);
	} else
		nvme_ctrlr_reset(ctrlr);
}

void
nvme_qpair_submit_tracker(struct nvme_qpair *qpair, struct nvme_tracker *tr)
{
	struct nvme_request	*req;
	struct nvme_controller	*ctrlr;

	mtx_assert(&qpair->lock, MA_OWNED);

	req = tr->req;
	req->cmd.cid = tr->cid;
	qpair->act_tr[tr->cid] = tr;
	ctrlr = qpair->ctrlr;

	if (req->timeout)
#if __FreeBSD_version >= 800030
		callout_reset_curcpu(&tr->timer, ctrlr->timeout_period * hz,
		    nvme_timeout, tr);
#else
		callout_reset(&tr->timer, ctrlr->timeout_period * hz,
		    nvme_timeout, tr);
#endif

	/* Copy the command from the tracker to the submission queue. */
	memcpy(&qpair->cmd[qpair->sq_tail], &req->cmd, sizeof(req->cmd));

	if (++qpair->sq_tail == qpair->num_entries)
		qpair->sq_tail = 0;

	wmb();
	nvme_mmio_write_4(qpair->ctrlr, doorbell[qpair->id].sq_tdbl,
	    qpair->sq_tail);

	qpair->num_cmds++;
}

static void
nvme_payload_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct nvme_tracker 	*tr = arg;
	uint32_t		cur_nseg;

	/*
	 * If the mapping operation failed, return immediately.  The caller
	 *  is responsible for detecting the error status and failing the
	 *  tracker manually.
	 */
	if (error != 0) {
		nvme_printf(tr->qpair->ctrlr,
		    "nvme_payload_map err %d\n", error);
		return;
	}

	/*
	 * Note that we specified PAGE_SIZE for alignment and max
	 *  segment size when creating the bus dma tags.  So here
	 *  we can safely just transfer each segment to its
	 *  associated PRP entry.
	 */
	tr->req->cmd.prp1 = seg[0].ds_addr;

	if (nseg == 2) {
		tr->req->cmd.prp2 = seg[1].ds_addr;
	} else if (nseg > 2) {
		cur_nseg = 1;
		tr->req->cmd.prp2 = (uint64_t)tr->prp_bus_addr;
		while (cur_nseg < nseg) {
			tr->prp[cur_nseg-1] =
			    (uint64_t)seg[cur_nseg].ds_addr;
			cur_nseg++;
		}
	} else {
		/*
		 * prp2 should not be used by the controller
		 *  since there is only one segment, but set
		 *  to 0 just to be safe.
		 */
		tr->req->cmd.prp2 = 0;
	}

	nvme_qpair_submit_tracker(tr->qpair, tr);
}

static void
_nvme_qpair_submit_request(struct nvme_qpair *qpair, struct nvme_request *req)
{
	struct nvme_tracker	*tr;
	int			err = 0;

	mtx_assert(&qpair->lock, MA_OWNED);

	tr = TAILQ_FIRST(&qpair->free_tr);
	req->qpair = qpair;

	if (tr == NULL || !qpair->is_enabled) {
		/*
		 * No tracker is available, or the qpair is disabled due to
		 *  an in-progress controller-level reset or controller
		 *  failure.
		 */

		if (qpair->ctrlr->is_failed) {
			/*
			 * The controller has failed.  Post the request to a
			 *  task where it will be aborted, so that we do not
			 *  invoke the request's callback in the context
			 *  of the submission.
			 */
			nvme_ctrlr_post_failed_request(qpair->ctrlr, req);
		} else {
			/*
			 * Put the request on the qpair's request queue to be
			 *  processed when a tracker frees up via a command
			 *  completion or when the controller reset is
			 *  completed.
			 */
			STAILQ_INSERT_TAIL(&qpair->queued_req, req, stailq);
		}
		return;
	}

	TAILQ_REMOVE(&qpair->free_tr, tr, tailq);
	TAILQ_INSERT_TAIL(&qpair->outstanding_tr, tr, tailq);
	tr->req = req;

	switch (req->type) {
	case NVME_REQUEST_VADDR:
		KASSERT(req->payload_size <= qpair->ctrlr->max_xfer_size,
		    ("payload_size (%d) exceeds max_xfer_size (%d)\n",
		    req->payload_size, qpair->ctrlr->max_xfer_size));
		err = bus_dmamap_load(tr->qpair->dma_tag_payload,
		    tr->payload_dma_map, req->u.payload, req->payload_size,
		    nvme_payload_map, tr, 0);
		if (err != 0)
			nvme_printf(qpair->ctrlr,
			    "bus_dmamap_load returned 0x%x!\n", err);
		break;
	case NVME_REQUEST_NULL:
		nvme_qpair_submit_tracker(tr->qpair, tr);
		break;
#ifdef NVME_UNMAPPED_BIO_SUPPORT
	case NVME_REQUEST_BIO:
		KASSERT(req->u.bio->bio_bcount <= qpair->ctrlr->max_xfer_size,
		    ("bio->bio_bcount (%jd) exceeds max_xfer_size (%d)\n",
		    (intmax_t)req->u.bio->bio_bcount,
		    qpair->ctrlr->max_xfer_size));
		err = bus_dmamap_load_bio(tr->qpair->dma_tag_payload,
		    tr->payload_dma_map, req->u.bio, nvme_payload_map, tr, 0);
		if (err != 0)
			nvme_printf(qpair->ctrlr,
			    "bus_dmamap_load_bio returned 0x%x!\n", err);
		break;
#endif
	default:
		panic("unknown nvme request type 0x%x\n", req->type);
		break;
	}

	if (err != 0) {
		/*
		 * The dmamap operation failed, so we manually fail the
		 *  tracker here with DATA_TRANSFER_ERROR status.
		 *
		 * nvme_qpair_manual_complete_tracker must not be called
		 *  with the qpair lock held.
		 */
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(qpair, tr, NVME_SCT_GENERIC,
		    NVME_SC_DATA_TRANSFER_ERROR, 1 /* do not retry */, TRUE);
		mtx_lock(&qpair->lock);
	}
}

void
nvme_qpair_submit_request(struct nvme_qpair *qpair, struct nvme_request *req)
{

	mtx_lock(&qpair->lock);
	_nvme_qpair_submit_request(qpair, req);
	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_enable(struct nvme_qpair *qpair)
{

	qpair->is_enabled = TRUE;
}

void
nvme_qpair_reset(struct nvme_qpair *qpair)
{

	qpair->sq_head = qpair->sq_tail = qpair->cq_head = 0;

	/*
	 * First time through the completion queue, HW will set phase
	 *  bit on completions to 1.  So set this to 1 here, indicating
	 *  we're looking for a 1 to know which entries have completed.
	 *  we'll toggle the bit each time when the completion queue
	 *  rolls over.
	 */
	qpair->phase = 1;

	memset(qpair->cmd, 0,
	    qpair->num_entries * sizeof(struct nvme_command));
	memset(qpair->cpl, 0,
	    qpair->num_entries * sizeof(struct nvme_completion));
}

void
nvme_admin_qpair_enable(struct nvme_qpair *qpair)
{
	struct nvme_tracker		*tr;
	struct nvme_tracker		*tr_temp;

	/*
	 * Manually abort each outstanding admin command.  Do not retry
	 *  admin commands found here, since they will be left over from
	 *  a controller reset and its likely the context in which the
	 *  command was issued no longer applies.
	 */
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_printf(qpair->ctrlr,
		    "aborting outstanding admin command\n");
		nvme_qpair_manual_complete_tracker(qpair, tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 1 /* do not retry */, TRUE);
	}

	nvme_qpair_enable(qpair);
}

void
nvme_io_qpair_enable(struct nvme_qpair *qpair)
{
	STAILQ_HEAD(, nvme_request)	temp;
	struct nvme_tracker		*tr;
	struct nvme_tracker		*tr_temp;
	struct nvme_request		*req;

	/*
	 * Manually abort each outstanding I/O.  This normally results in a
	 *  retry, unless the retry count on the associated request has
	 *  reached its limit.
	 */
	TAILQ_FOREACH_SAFE(tr, &qpair->outstanding_tr, tailq, tr_temp) {
		nvme_printf(qpair->ctrlr, "aborting outstanding i/o\n");
		nvme_qpair_manual_complete_tracker(qpair, tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 0, TRUE);
	}

	mtx_lock(&qpair->lock);

	nvme_qpair_enable(qpair);

	STAILQ_INIT(&temp);
	STAILQ_SWAP(&qpair->queued_req, &temp, nvme_request);

	while (!STAILQ_EMPTY(&temp)) {
		req = STAILQ_FIRST(&temp);
		STAILQ_REMOVE_HEAD(&temp, stailq);
		nvme_printf(qpair->ctrlr, "resubmitting queued i/o\n");
		nvme_qpair_print_command(qpair, &req->cmd);
		_nvme_qpair_submit_request(qpair, req);
	}

	mtx_unlock(&qpair->lock);
}

static void
nvme_qpair_disable(struct nvme_qpair *qpair)
{
	struct nvme_tracker *tr;

	qpair->is_enabled = FALSE;
	mtx_lock(&qpair->lock);
	TAILQ_FOREACH(tr, &qpair->outstanding_tr, tailq)
		callout_stop(&tr->timer);
	mtx_unlock(&qpair->lock);
}

void
nvme_admin_qpair_disable(struct nvme_qpair *qpair)
{

	nvme_qpair_disable(qpair);
	nvme_admin_qpair_abort_aers(qpair);
}

void
nvme_io_qpair_disable(struct nvme_qpair *qpair)
{

	nvme_qpair_disable(qpair);
}

void
nvme_qpair_fail(struct nvme_qpair *qpair)
{
	struct nvme_tracker		*tr;
	struct nvme_request		*req;

	mtx_lock(&qpair->lock);

	while (!STAILQ_EMPTY(&qpair->queued_req)) {
		req = STAILQ_FIRST(&qpair->queued_req);
		STAILQ_REMOVE_HEAD(&qpair->queued_req, stailq);
		nvme_printf(qpair->ctrlr, "failing queued i/o\n");
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_request(qpair, req, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, TRUE);
		mtx_lock(&qpair->lock);
	}

	/* Manually abort each outstanding I/O. */
	while (!TAILQ_EMPTY(&qpair->outstanding_tr)) {
		tr = TAILQ_FIRST(&qpair->outstanding_tr);
		/*
		 * Do not remove the tracker.  The abort_tracker path will
		 *  do that for us.
		 */
		nvme_printf(qpair->ctrlr, "failing outstanding i/o\n");
		mtx_unlock(&qpair->lock);
		nvme_qpair_manual_complete_tracker(qpair, tr, NVME_SCT_GENERIC,
		    NVME_SC_ABORTED_BY_REQUEST, 1 /* do not retry */, TRUE);
		mtx_lock(&qpair->lock);
	}

	mtx_unlock(&qpair->lock);
}

