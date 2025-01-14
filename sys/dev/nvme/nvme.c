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
__FBSDID("$FreeBSD: head/sys/dev/nvme/nvme.c 293328 2016-01-07 16:18:32Z jimharris $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <vm/uma.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "nvme_private.h"

struct nvme_consumer {
	uint32_t		id;
	nvme_cons_ns_fn_t	ns_fn;
	nvme_cons_ctrlr_fn_t	ctrlr_fn;
	nvme_cons_async_fn_t	async_fn;
	nvme_cons_fail_fn_t	fail_fn;
};

struct nvme_consumer nvme_consumer[NVME_MAX_CONSUMERS];
#define	INVALID_CONSUMER_ID	0xFFFF

uma_zone_t	nvme_request_zone;
int32_t		nvme_retry_count;

MALLOC_DEFINE(M_NVME, "nvme", "nvme(4) memory allocations");

static int    nvme_probe(device_t);
static int    nvme_attach(device_t);
static int    nvme_detach(device_t);
static int    nvme_modevent(module_t mod, int type, void *arg);

static devclass_t nvme_devclass;

static device_method_t nvme_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     nvme_probe),
	DEVMETHOD(device_attach,    nvme_attach),
	DEVMETHOD(device_detach,    nvme_detach),
	{ 0, 0 }
};

static driver_t nvme_pci_driver = {
	"nvme",
	nvme_pci_methods,
	sizeof(struct nvme_controller),
};

DRIVER_MODULE(nvme, pci, nvme_pci_driver, nvme_devclass, nvme_modevent, 0);
MODULE_VERSION(nvme, 1);

static struct _pcsid
{
	uint32_t	devid;
	int		match_subdevice;
	uint16_t	subdevice;
	const char	*desc;
} pci_ids[] = {
	{ 0x01118086,		0, 0, "NVMe Controller"  },
	{ IDT32_PCI_ID,		0, 0, "IDT NVMe Controller (32 channel)"  },
	{ IDT8_PCI_ID,		0, 0, "IDT NVMe Controller (8 channel)" },
	{ 0x09538086,		1, 0x3702, "DC P3700 SSD" },
	{ 0x09538086,		1, 0x3703, "DC P3700 SSD [2.5\" SFF]" },
	{ 0x09538086,		1, 0x3704, "DC P3500 SSD [Add-in Card]" },
	{ 0x09538086,		1, 0x3705, "DC P3500 SSD [2.5\" SFF]" },
	{ 0x09538086,		1, 0x3709, "DC P3600 SSD [Add-in Card]" },
	{ 0x09538086,		1, 0x370a, "DC P3600 SSD [2.5\" SFF]" },
	{ 0x00000000,		0, 0, NULL  }
};

static int
nvme_match(uint32_t devid, uint16_t subdevice, struct _pcsid *ep)
{
	if (devid != ep->devid)
		return 0;

	if (!ep->match_subdevice)
		return 1;

	if (subdevice == ep->subdevice)
		return 1;
	else
		return 0;
}

static int
nvme_probe (device_t device)
{
	struct _pcsid	*ep;
	uint32_t	devid;
	uint16_t	subdevice;

	devid = pci_get_devid(device);
	subdevice = pci_get_subdevice(device);
	ep = pci_ids;

	while (ep->devid) {
		if (nvme_match(devid, subdevice, ep))
			break;
		++ep;
	}

	if (ep->desc) {
		device_set_desc(device, ep->desc);
		return (BUS_PROBE_DEFAULT);
	}

#if defined(PCIS_STORAGE_NVM)
	if (pci_get_class(device)    == PCIC_STORAGE &&
	    pci_get_subclass(device) == PCIS_STORAGE_NVM &&
	    pci_get_progif(device)   == PCIP_STORAGE_NVM_ENTERPRISE_NVMHCI_1_0) {
		device_set_desc(device, "Generic NVMe Device");
		return (BUS_PROBE_GENERIC);
	}
#endif

	return (ENXIO);
}

static void
nvme_init(void)
{
	uint32_t	i;

	nvme_request_zone = uma_zcreate("nvme_request",
	    sizeof(struct nvme_request), NULL, NULL, NULL, NULL, 0, 0);

	for (i = 0; i < NVME_MAX_CONSUMERS; i++)
		nvme_consumer[i].id = INVALID_CONSUMER_ID;
}

SYSINIT(nvme_register, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_init, NULL);

static void
nvme_uninit(void)
{
	uma_zdestroy(nvme_request_zone);
}

SYSUNINIT(nvme_unregister, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_uninit, NULL);

static void
nvme_load(void)
{
}

static void
nvme_unload(void)
{
}

static void
nvme_shutdown(void)
{
	device_t		*devlist;
	struct nvme_controller	*ctrlr;
	int			dev, devcount;

	if (devclass_get_devices(nvme_devclass, &devlist, &devcount))
		return;

	for (dev = 0; dev < devcount; dev++) {
		ctrlr = DEVICE2SOFTC(devlist[dev]);
		nvme_ctrlr_shutdown(ctrlr);
	}

	free(devlist, M_TEMP);
}

static int
nvme_modevent(module_t mod, int type, void *arg)
{

	switch (type) {
	case MOD_LOAD:
		nvme_load();
		break;
	case MOD_UNLOAD:
		nvme_unload();
		break;
	case MOD_SHUTDOWN:
		nvme_shutdown();
		break;
	default:
		break;
	}

	return (0);
}

void
nvme_dump_command(struct nvme_command *cmd)
{
	printf(
"opc:%x f:%x r1:%x cid:%x nsid:%x r2:%x r3:%x mptr:%jx prp1:%jx prp2:%jx cdw:%x %x %x %x %x %x\n",
	    cmd->opc, cmd->fuse, cmd->rsvd1, cmd->cid, cmd->nsid,
	    cmd->rsvd2, cmd->rsvd3,
	    (uintmax_t)cmd->mptr, (uintmax_t)cmd->prp1, (uintmax_t)cmd->prp2,
	    cmd->cdw10, cmd->cdw11, cmd->cdw12, cmd->cdw13, cmd->cdw14,
	    cmd->cdw15);
}

void
nvme_dump_completion(struct nvme_completion *cpl)
{
	printf("cdw0:%08x sqhd:%04x sqid:%04x "
	    "cid:%04x p:%x sc:%02x sct:%x m:%x dnr:%x\n",
	    cpl->cdw0, cpl->sqhd, cpl->sqid,
	    cpl->cid, cpl->status.p, cpl->status.sc, cpl->status.sct,
	    cpl->status.m, cpl->status.dnr);
}

static int
nvme_attach(device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);
	int			status;

	status = nvme_ctrlr_construct(ctrlr, dev);

	if (status != 0) {
		nvme_ctrlr_destruct(ctrlr, dev);
		return (status);
	}

	/*
	 * Reset controller twice to ensure we do a transition from cc.en==1
	 *  to cc.en==0.  This is because we don't really know what status
	 *  the controller was left in when boot handed off to OS.
	 */
	status = nvme_ctrlr_hw_reset(ctrlr);
	if (status != 0) {
		nvme_ctrlr_destruct(ctrlr, dev);
		return (status);
	}

	status = nvme_ctrlr_hw_reset(ctrlr);
	if (status != 0) {
		nvme_ctrlr_destruct(ctrlr, dev);
		return (status);
	}

	pci_enable_busmaster(dev);

	ctrlr->config_hook.ich_func = nvme_ctrlr_start_config_hook;
	ctrlr->config_hook.ich_arg = ctrlr;

	config_intrhook_establish(&ctrlr->config_hook);

	return (0);
}

static int
nvme_detach (device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);

	nvme_ctrlr_destruct(ctrlr, dev);
	pci_disable_busmaster(dev);
	return (0);
}

static void
nvme_notify(struct nvme_consumer *cons,
	    struct nvme_controller *ctrlr)
{
	struct nvme_namespace	*ns;
	void			*ctrlr_cookie;
	int			cmpset, ns_idx;

	/*
	 * The consumer may register itself after the nvme devices
	 *  have registered with the kernel, but before the
	 *  driver has completed initialization.  In that case,
	 *  return here, and when initialization completes, the
	 *  controller will make sure the consumer gets notified.
	 */
	if (!ctrlr->is_initialized)
		return;

	cmpset = atomic_cmpset_32(&ctrlr->notification_sent, 0, 1);

	if (cmpset == 0)
		return;

	if (cons->ctrlr_fn != NULL)
		ctrlr_cookie = (*cons->ctrlr_fn)(ctrlr);
	else
		ctrlr_cookie = NULL;
	ctrlr->cons_cookie[cons->id] = ctrlr_cookie;
	if (ctrlr->is_failed) {
		if (cons->fail_fn != NULL)
			(*cons->fail_fn)(ctrlr_cookie);
		/*
		 * Do not notify consumers about the namespaces of a
		 *  failed controller.
		 */
		return;
	}
	for (ns_idx = 0; ns_idx < ctrlr->cdata.nn; ns_idx++) {
		ns = &ctrlr->ns[ns_idx];
		if (cons->ns_fn != NULL)
			ns->cons_cookie[cons->id] =
			    (*cons->ns_fn)(ns, ctrlr_cookie);
	}
}

void
nvme_notify_new_controller(struct nvme_controller *ctrlr)
{
	int i;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		if (nvme_consumer[i].id != INVALID_CONSUMER_ID) {
			nvme_notify(&nvme_consumer[i], ctrlr);
		}
	}
}

static void
nvme_notify_new_consumer(struct nvme_consumer *cons)
{
	device_t		*devlist;
	struct nvme_controller	*ctrlr;
	int			dev_idx, devcount;

	if (devclass_get_devices(nvme_devclass, &devlist, &devcount))
		return;

	for (dev_idx = 0; dev_idx < devcount; dev_idx++) {
		ctrlr = DEVICE2SOFTC(devlist[dev_idx]);
		nvme_notify(cons, ctrlr);
	}

	free(devlist, M_TEMP);
}

void
nvme_notify_async_consumers(struct nvme_controller *ctrlr,
			    const struct nvme_completion *async_cpl,
			    uint32_t log_page_id, void *log_page_buffer,
			    uint32_t log_page_size)
{
	struct nvme_consumer	*cons;
	uint32_t		i;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		cons = &nvme_consumer[i];
		if (cons->id != INVALID_CONSUMER_ID && cons->async_fn != NULL)
			(*cons->async_fn)(ctrlr->cons_cookie[i], async_cpl,
			    log_page_id, log_page_buffer, log_page_size);
	}
}

void
nvme_notify_fail_consumers(struct nvme_controller *ctrlr)
{
	struct nvme_consumer	*cons;
	uint32_t		i;

	/*
	 * This controller failed during initialization (i.e. IDENTIFY
	 *  command failed or timed out).  Do not notify any nvme
	 *  consumers of the failure here, since the consumer does not
	 *  even know about the controller yet.
	 */
	if (!ctrlr->is_initialized)
		return;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		cons = &nvme_consumer[i];
		if (cons->id != INVALID_CONSUMER_ID && cons->fail_fn != NULL)
			cons->fail_fn(ctrlr->cons_cookie[i]);
	}
}

struct nvme_consumer *
nvme_register_consumer(nvme_cons_ns_fn_t ns_fn, nvme_cons_ctrlr_fn_t ctrlr_fn,
		       nvme_cons_async_fn_t async_fn,
		       nvme_cons_fail_fn_t fail_fn)
{
	int i;

	/*
	 * TODO: add locking around consumer registration.  Not an issue
	 *  right now since we only have one nvme consumer - nvd(4).
	 */
	for (i = 0; i < NVME_MAX_CONSUMERS; i++)
		if (nvme_consumer[i].id == INVALID_CONSUMER_ID) {
			nvme_consumer[i].id = i;
			nvme_consumer[i].ns_fn = ns_fn;
			nvme_consumer[i].ctrlr_fn = ctrlr_fn;
			nvme_consumer[i].async_fn = async_fn;
			nvme_consumer[i].fail_fn = fail_fn;

			nvme_notify_new_consumer(&nvme_consumer[i]);
			return (&nvme_consumer[i]);
		}

	printf("nvme(4): consumer not registered - no slots available\n");
	return (NULL);
}

void
nvme_unregister_consumer(struct nvme_consumer *consumer)
{

	consumer->id = INVALID_CONSUMER_ID;
}

void
nvme_completion_poll_cb(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_completion_poll_status	*status = arg;

	/*
	 * Copy status into the argument passed by the caller, so that
	 *  the caller can check the status to determine if the
	 *  the request passed or failed.
	 */
	memcpy(&status->cpl, cpl, sizeof(*cpl));
	wmb();
	status->done = TRUE;
}
