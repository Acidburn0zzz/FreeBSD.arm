/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/dev/mlx5/mlx5_core/mlx5_qp.c 290650 2015-11-10 12:20:22Z hselasky $
 */


#include <linux/gfp.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/driver.h>

#include "mlx5_core.h"

static struct mlx5_core_rsc_common *mlx5_get_rsc(struct mlx5_core_dev *dev,
						 u32 rsn)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	struct mlx5_core_rsc_common *common;

	spin_lock(&table->lock);

	common = radix_tree_lookup(&table->tree, rsn);
	if (common)
		atomic_inc(&common->refcount);

	spin_unlock(&table->lock);

	if (!common) {
		mlx5_core_warn(dev, "Async event for bogus resource 0x%x\n",
			       rsn);
		return NULL;
	}
	return common;
}

void mlx5_core_put_rsc(struct mlx5_core_rsc_common *common)
{
	if (atomic_dec_and_test(&common->refcount))
		complete(&common->free);
}

void mlx5_rsc_event(struct mlx5_core_dev *dev, u32 rsn, int event_type)
{
	struct mlx5_core_rsc_common *common = mlx5_get_rsc(dev, rsn);
	struct mlx5_core_qp *qp;

	if (!common)
		return;

	switch (common->res) {
	case MLX5_RES_QP:
		qp = (struct mlx5_core_qp *)common;
		qp->event(qp, event_type);
		break;

	default:
		mlx5_core_warn(dev, "invalid resource type for 0x%x\n", rsn);
	}

	mlx5_core_put_rsc(common);
}

int mlx5_core_create_qp(struct mlx5_core_dev *dev,
			struct mlx5_core_qp *qp,
			struct mlx5_create_qp_mbox_in *in,
			int inlen)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	struct mlx5_create_qp_mbox_out out;
	struct mlx5_destroy_qp_mbox_in din;
	struct mlx5_destroy_qp_mbox_out dout;
	int err;
	void *qpc;

	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_QP);
	if (dev->issi) {
		qpc = MLX5_ADDR_OF(create_qp_in, in, qpc);
		/* 0xffffff means we ask to work with cqe version 0 */
		MLX5_SET(qpc, qpc, user_index, 0xffffff);
	}

	err = mlx5_cmd_exec(dev, in, inlen, &out, sizeof(out));
	if (err) {
		mlx5_core_warn(dev, "ret %d\n", err);
		return err;
	}

	if (out.hdr.status) {
		mlx5_core_warn(dev, "current num of QPs 0x%x\n",
			       atomic_read(&dev->num_qps));
		return mlx5_cmd_status_to_err(&out.hdr);
	}

	qp->qpn = be32_to_cpu(out.qpn) & 0xffffff;
	mlx5_core_dbg(dev, "qpn = 0x%x\n", qp->qpn);

	qp->common.res = MLX5_RES_QP;
	spin_lock_irq(&table->lock);
	err = radix_tree_insert(&table->tree, qp->qpn, qp);
	spin_unlock_irq(&table->lock);
	if (err) {
		mlx5_core_warn(dev, "err %d\n", err);
		goto err_cmd;
	}

	qp->pid = curthread->td_proc->p_pid;
	atomic_set(&qp->common.refcount, 1);
	atomic_inc(&dev->num_qps);
	init_completion(&qp->common.free);

	return 0;

err_cmd:
	memset(&din, 0, sizeof(din));
	memset(&dout, 0, sizeof(dout));
	din.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_QP);
	din.qpn = cpu_to_be32(qp->qpn);
	mlx5_cmd_exec(dev, &din, sizeof(din), &out, sizeof(dout));

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_create_qp);

int mlx5_core_destroy_qp(struct mlx5_core_dev *dev,
			 struct mlx5_core_qp *qp)
{
	struct mlx5_destroy_qp_mbox_in in;
	struct mlx5_destroy_qp_mbox_out out;
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	unsigned long flags;
	int err;


	spin_lock_irqsave(&table->lock, flags);
	radix_tree_delete(&table->tree, qp->qpn);
	spin_unlock_irqrestore(&table->lock, flags);

	mlx5_core_put_rsc((struct mlx5_core_rsc_common *)qp);
	wait_for_completion(&qp->common.free);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_QP);
	in.qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	atomic_dec(&dev->num_qps);
	return 0;
}
EXPORT_SYMBOL_GPL(mlx5_core_destroy_qp);

int mlx5_core_qp_modify(struct mlx5_core_dev *dev, enum mlx5_qp_state cur_state,
			enum mlx5_qp_state new_state,
			struct mlx5_modify_qp_mbox_in *in, int sqd_event,
			struct mlx5_core_qp *qp)
{
	static const u16 optab[MLX5_QP_NUM_STATE][MLX5_QP_NUM_STATE] = {
		[MLX5_QP_STATE_RST] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_INIT]	= MLX5_CMD_OP_RST2INIT_QP,
		},
		[MLX5_QP_STATE_INIT]  = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_INIT]	= MLX5_CMD_OP_INIT2INIT_QP,
			[MLX5_QP_STATE_RTR]	= MLX5_CMD_OP_INIT2RTR_QP,
		},
		[MLX5_QP_STATE_RTR]   = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_RTR2RTS_QP,
		},
		[MLX5_QP_STATE_RTS]   = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_RTS2RTS_QP,
		},
		[MLX5_QP_STATE_SQD] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
		},
		[MLX5_QP_STATE_SQER] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_SQERR2RTS_QP,
		},
		[MLX5_QP_STATE_ERR] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
		}
	};

	struct mlx5_modify_qp_mbox_out out;
	int err = 0;
	u16 op;

	if (cur_state >= MLX5_QP_NUM_STATE || new_state >= MLX5_QP_NUM_STATE ||
	    !optab[cur_state][new_state])
		return -EINVAL;

	memset(&out, 0, sizeof(out));
	op = optab[cur_state][new_state];
	in->hdr.opcode = cpu_to_be16(op);
	in->qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, in, sizeof(*in), &out, sizeof(out));
	if (err)
		return err;

	return mlx5_cmd_status_to_err(&out.hdr);
}
EXPORT_SYMBOL_GPL(mlx5_core_qp_modify);

void mlx5_init_qp_table(struct mlx5_core_dev *dev)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;

	spin_lock_init(&table->lock);
	INIT_RADIX_TREE(&table->tree, GFP_ATOMIC);
}

void mlx5_cleanup_qp_table(struct mlx5_core_dev *dev)
{
}

int mlx5_core_qp_query(struct mlx5_core_dev *dev, struct mlx5_core_qp *qp,
		       struct mlx5_query_qp_mbox_out *out, int outlen)
{
	struct mlx5_query_qp_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(out, 0, outlen);
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_QP);
	in.qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, outlen);
	if (err)
		return err;

	if (out->hdr.status)
		return mlx5_cmd_status_to_err(&out->hdr);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_qp_query);

int mlx5_core_xrcd_alloc(struct mlx5_core_dev *dev, u32 *xrcdn)
{
	u32 in[MLX5_ST_SZ_DW(alloc_xrcd_in)];
	u32 out[MLX5_ST_SZ_DW(alloc_xrcd_out)];
	int err;

	memset(in, 0, sizeof(in));

	MLX5_SET(alloc_xrcd_in, in, opcode, MLX5_CMD_OP_ALLOC_XRCD);

	memset(out, 0, sizeof(out));
	err = mlx5_cmd_exec_check_status(dev, in, sizeof(in), out, sizeof(out));
	if (err)
		return err;

	*xrcdn = MLX5_GET(alloc_xrcd_out, out, xrcd);
	return 0;
}
EXPORT_SYMBOL_GPL(mlx5_core_xrcd_alloc);

int mlx5_core_xrcd_dealloc(struct mlx5_core_dev *dev, u32 xrcdn)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_xrcd_in)];
	u32 out[MLX5_ST_SZ_DW(dealloc_xrcd_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(dealloc_xrcd_in, in, opcode, MLX5_CMD_OP_DEALLOC_XRCD);
	MLX5_SET(dealloc_xrcd_in, in, xrcd, xrcdn);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}
EXPORT_SYMBOL_GPL(mlx5_core_xrcd_dealloc);
