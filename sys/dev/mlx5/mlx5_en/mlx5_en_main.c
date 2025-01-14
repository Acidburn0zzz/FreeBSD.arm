/*-
 * Copyright (c) 2015 Mellanox Technologies. All rights reserved.
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
 * $FreeBSD: head/sys/dev/mlx5/mlx5_en/mlx5_en_main.c 301545 2016-06-07 13:58:52Z hselasky $
 */

#include "en.h"

#include <sys/sockio.h>
#include <machine/atomic.h>

#define	ETH_DRIVER_VERSION	"3.1.0-dev"
char mlx5e_version[] = "Mellanox Ethernet driver"
    " (" ETH_DRIVER_VERSION ")";

struct mlx5e_rq_param {
	u32	rqc [MLX5_ST_SZ_DW(rqc)];
	struct mlx5_wq_param wq;
};

struct mlx5e_sq_param {
	u32	sqc [MLX5_ST_SZ_DW(sqc)];
	struct mlx5_wq_param wq;
};

struct mlx5e_cq_param {
	u32	cqc [MLX5_ST_SZ_DW(cqc)];
	struct mlx5_wq_param wq;
	u16	eq_ix;
};

struct mlx5e_channel_param {
	struct mlx5e_rq_param rq;
	struct mlx5e_sq_param sq;
	struct mlx5e_cq_param rx_cq;
	struct mlx5e_cq_param tx_cq;
};

static const struct {
	u32	subtype;
	u64	baudrate;
}	mlx5e_mode_table[MLX5E_LINK_MODES_NUMBER] = {

	[MLX5E_1000BASE_CX_SGMII] = {
		.subtype = IFM_1000_CX_SGMII,
		.baudrate = IF_Mbps(1000ULL),
	},
	[MLX5E_1000BASE_KX] = {
		.subtype = IFM_1000_KX,
		.baudrate = IF_Mbps(1000ULL),
	},
	[MLX5E_10GBASE_CX4] = {
		.subtype = IFM_10G_CX4,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_KX4] = {
		.subtype = IFM_10G_KX4,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_KR] = {
		.subtype = IFM_10G_KR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_20GBASE_KR2] = {
		.subtype = IFM_20G_KR2,
		.baudrate = IF_Gbps(20ULL),
	},
	[MLX5E_40GBASE_CR4] = {
		.subtype = IFM_40G_CR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_KR4] = {
		.subtype = IFM_40G_KR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_56GBASE_R4] = {
		.subtype = IFM_56G_R4,
		.baudrate = IF_Gbps(56ULL),
	},
	[MLX5E_10GBASE_CR] = {
		.subtype = IFM_10G_CR1,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_SR] = {
		.subtype = IFM_10G_SR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_10GBASE_LR] = {
		.subtype = IFM_10G_LR,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_40GBASE_SR4] = {
		.subtype = IFM_40G_SR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_40GBASE_LR4] = {
		.subtype = IFM_40G_LR4,
		.baudrate = IF_Gbps(40ULL),
	},
	[MLX5E_100GBASE_CR4] = {
		.subtype = IFM_100G_CR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_SR4] = {
		.subtype = IFM_100G_SR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_KR4] = {
		.subtype = IFM_100G_KR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100GBASE_LR4] = {
		.subtype = IFM_100G_LR4,
		.baudrate = IF_Gbps(100ULL),
	},
	[MLX5E_100BASE_TX] = {
		.subtype = IFM_100_TX,
		.baudrate = IF_Mbps(100ULL),
	},
	[MLX5E_100BASE_T] = {
		.subtype = IFM_100_T,
		.baudrate = IF_Mbps(100ULL),
	},
	[MLX5E_10GBASE_T] = {
		.subtype = IFM_10G_T,
		.baudrate = IF_Gbps(10ULL),
	},
	[MLX5E_25GBASE_CR] = {
		.subtype = IFM_25G_CR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GBASE_KR] = {
		.subtype = IFM_25G_KR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_25GBASE_SR] = {
		.subtype = IFM_25G_SR,
		.baudrate = IF_Gbps(25ULL),
	},
	[MLX5E_50GBASE_CR2] = {
		.subtype = IFM_50G_CR2,
		.baudrate = IF_Gbps(50ULL),
	},
	[MLX5E_50GBASE_KR2] = {
		.subtype = IFM_50G_KR2,
		.baudrate = IF_Gbps(50ULL),
	},
};

MALLOC_DEFINE(M_MLX5EN, "MLX5EN", "MLX5 Ethernet");

static void
mlx5e_update_carrier(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 out[MLX5_ST_SZ_DW(ptys_reg)];
	u32 eth_proto_oper;
	int error;
	u8 port_state;
	u8 i;

	port_state = mlx5_query_vport_state(mdev,
	    MLX5_QUERY_VPORT_STATE_IN_OP_MOD_VNIC_VPORT);

	if (port_state == VPORT_STATE_UP) {
		priv->media_status_last |= IFM_ACTIVE;
	} else {
		priv->media_status_last &= ~IFM_ACTIVE;
		priv->media_active_last = IFM_ETHER;
		if_link_state_change(priv->ifp, LINK_STATE_DOWN);
		return;
	}

	error = mlx5_query_port_ptys(mdev, out, sizeof(out), MLX5_PTYS_EN);
	if (error) {
		priv->media_active_last = IFM_ETHER;
		priv->ifp->if_baudrate = 1;
		if_printf(priv->ifp, "%s: query port ptys failed: 0x%x\n",
		    __func__, error);
		return;
	}
	eth_proto_oper = MLX5_GET(ptys_reg, out, eth_proto_oper);

	for (i = 0; i != MLX5E_LINK_MODES_NUMBER; i++) {
		if (mlx5e_mode_table[i].baudrate == 0)
			continue;
		if (MLX5E_PROT_MASK(i) & eth_proto_oper) {
			priv->ifp->if_baudrate =
			    mlx5e_mode_table[i].baudrate;
			priv->media_active_last =
			    mlx5e_mode_table[i].subtype | IFM_ETHER | IFM_FDX;
		}
	}
	if_link_state_change(priv->ifp, LINK_STATE_UP);
}

static void
mlx5e_media_status(struct ifnet *dev, struct ifmediareq *ifmr)
{
	struct mlx5e_priv *priv = dev->if_softc;

	ifmr->ifm_status = priv->media_status_last;
	ifmr->ifm_active = priv->media_active_last |
	    (priv->params.rx_pauseframe_control ? IFM_ETH_RXPAUSE : 0) |
	    (priv->params.tx_pauseframe_control ? IFM_ETH_TXPAUSE : 0);

}

static u32
mlx5e_find_link_mode(u32 subtype)
{
	u32 i;
	u32 link_mode = 0;

	for (i = 0; i < MLX5E_LINK_MODES_NUMBER; ++i) {
		if (mlx5e_mode_table[i].baudrate == 0)
			continue;
		if (mlx5e_mode_table[i].subtype == subtype)
			link_mode |= MLX5E_PROT_MASK(i);
	}

	return (link_mode);
}

static int
mlx5e_media_change(struct ifnet *dev)
{
	struct mlx5e_priv *priv = dev->if_softc;
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 eth_proto_cap;
	u32 link_mode;
	int was_opened;
	int locked;
	int error;

	locked = PRIV_LOCKED(priv);
	if (!locked)
		PRIV_LOCK(priv);

	if (IFM_TYPE(priv->media.ifm_media) != IFM_ETHER) {
		error = EINVAL;
		goto done;
	}
	link_mode = mlx5e_find_link_mode(IFM_SUBTYPE(priv->media.ifm_media));

	/* query supported capabilities */
	error = mlx5_query_port_proto_cap(mdev, &eth_proto_cap, MLX5_PTYS_EN);
	if (error != 0) {
		if_printf(dev, "Query port media capability failed\n");
		goto done;
	}
	/* check for autoselect */
	if (IFM_SUBTYPE(priv->media.ifm_media) == IFM_AUTO) {
		link_mode = eth_proto_cap;
		if (link_mode == 0) {
			if_printf(dev, "Port media capability is zero\n");
			error = EINVAL;
			goto done;
		}
	} else {
		link_mode = link_mode & eth_proto_cap;
		if (link_mode == 0) {
			if_printf(dev, "Not supported link mode requested\n");
			error = EINVAL;
			goto done;
		}
	}
	/* update pauseframe control bits */
	priv->params.rx_pauseframe_control =
	    (priv->media.ifm_media & IFM_ETH_RXPAUSE) ? 1 : 0;
	priv->params.tx_pauseframe_control =
	    (priv->media.ifm_media & IFM_ETH_TXPAUSE) ? 1 : 0;

	/* check if device is opened */
	was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);

	/* reconfigure the hardware */
	mlx5_set_port_status(mdev, MLX5_PORT_DOWN);
	mlx5_set_port_proto(mdev, link_mode, MLX5_PTYS_EN);
	mlx5_set_port_pause(mdev, 1,
	    priv->params.rx_pauseframe_control,
	    priv->params.tx_pauseframe_control);
	if (was_opened)
		mlx5_set_port_status(mdev, MLX5_PORT_UP);

done:
	if (!locked)
		PRIV_UNLOCK(priv);
	return (error);
}

static void
mlx5e_update_carrier_work(struct work_struct *work)
{
	struct mlx5e_priv *priv = container_of(work, struct mlx5e_priv,
	    update_carrier_work);

	PRIV_LOCK(priv);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_update_carrier(priv);
	PRIV_UNLOCK(priv);
}

static void
mlx5e_update_pport_counters(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_pport_stats *s = &priv->stats.pport;
	struct mlx5e_port_stats_debug *s_debug = &priv->stats.port_stats_debug;
	u32 *in;
	u32 *out;
	u64 *ptr;
	unsigned sz = MLX5_ST_SZ_BYTES(ppcnt_reg);
	unsigned x;
	unsigned y;

	in = mlx5_vzalloc(sz);
	out = mlx5_vzalloc(sz);
	if (in == NULL || out == NULL)
		goto free_out;

	ptr = (uint64_t *)MLX5_ADDR_OF(ppcnt_reg, out, counter_set);

	MLX5_SET(ppcnt_reg, in, local_port, 1);

	MLX5_SET(ppcnt_reg, in, grp, MLX5_IEEE_802_3_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = y = 0; x != MLX5E_PPORT_IEEE802_3_STATS_NUM; x++, y++)
		s->arg[y] = be64toh(ptr[x]);

	MLX5_SET(ppcnt_reg, in, grp, MLX5_RFC_2819_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_RFC2819_STATS_NUM; x++, y++)
		s->arg[y] = be64toh(ptr[x]);
	for (y = 0; x != MLX5E_PPORT_RFC2819_STATS_NUM +
	    MLX5E_PPORT_RFC2819_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	MLX5_SET(ppcnt_reg, in, grp, MLX5_RFC_2863_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_RFC2863_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);

	MLX5_SET(ppcnt_reg, in, grp, MLX5_PHYSICAL_LAYER_COUNTERS_GROUP);
	mlx5_core_access_reg(mdev, in, sz, out, sz, MLX5_REG_PPCNT, 0, 0);
	for (x = 0; x != MLX5E_PPORT_PHYSICAL_LAYER_STATS_DEBUG_NUM; x++, y++)
		s_debug->arg[y] = be64toh(ptr[x]);
free_out:
	kvfree(in);
	kvfree(out);
}

static void
mlx5e_update_stats_work(struct work_struct *work)
{
	struct mlx5e_priv *priv = container_of(work, struct mlx5e_priv,
	    update_stats_work);
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_vport_stats *s = &priv->stats.vport;
	struct mlx5e_rq_stats *rq_stats;
	struct mlx5e_sq_stats *sq_stats;
	struct buf_ring *sq_br;
#if (__FreeBSD_version < 1100000)
	struct ifnet *ifp = priv->ifp;
#endif

	u32 in[MLX5_ST_SZ_DW(query_vport_counter_in)];
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_vport_counter_out);
	u64 tso_packets = 0;
	u64 tso_bytes = 0;
	u64 tx_queue_dropped = 0;
	u64 tx_defragged = 0;
	u64 tx_offload_none = 0;
	u64 lro_packets = 0;
	u64 lro_bytes = 0;
	u64 sw_lro_queued = 0;
	u64 sw_lro_flushed = 0;
	u64 rx_csum_none = 0;
	u64 rx_wqe_err = 0;
	u32 rx_out_of_buffer = 0;
	int i;
	int j;

	PRIV_LOCK(priv);
	out = mlx5_vzalloc(outlen);
	if (out == NULL)
		goto free_out;
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		goto free_out;

	/* Collect firts the SW counters and then HW for consistency */
	for (i = 0; i < priv->params.num_channels; i++) {
		struct mlx5e_rq *rq = &priv->channel[i]->rq;

		rq_stats = &priv->channel[i]->rq.stats;

		/* collect stats from LRO */
		rq_stats->sw_lro_queued = rq->lro.lro_queued;
		rq_stats->sw_lro_flushed = rq->lro.lro_flushed;
		sw_lro_queued += rq_stats->sw_lro_queued;
		sw_lro_flushed += rq_stats->sw_lro_flushed;
		lro_packets += rq_stats->lro_packets;
		lro_bytes += rq_stats->lro_bytes;
		rx_csum_none += rq_stats->csum_none;
		rx_wqe_err += rq_stats->wqe_err;

		for (j = 0; j < priv->num_tc; j++) {
			sq_stats = &priv->channel[i]->sq[j].stats;
			sq_br = priv->channel[i]->sq[j].br;

			tso_packets += sq_stats->tso_packets;
			tso_bytes += sq_stats->tso_bytes;
			tx_queue_dropped += sq_stats->dropped;
			tx_queue_dropped += sq_br->br_drops;
			tx_defragged += sq_stats->defragged;
			tx_offload_none += sq_stats->csum_offload_none;
		}
	}

	/* update counters */
	s->tso_packets = tso_packets;
	s->tso_bytes = tso_bytes;
	s->tx_queue_dropped = tx_queue_dropped;
	s->tx_defragged = tx_defragged;
	s->lro_packets = lro_packets;
	s->lro_bytes = lro_bytes;
	s->sw_lro_queued = sw_lro_queued;
	s->sw_lro_flushed = sw_lro_flushed;
	s->rx_csum_none = rx_csum_none;
	s->rx_wqe_err = rx_wqe_err;

	/* HW counters */
	memset(in, 0, sizeof(in));

	MLX5_SET(query_vport_counter_in, in, opcode,
	    MLX5_CMD_OP_QUERY_VPORT_COUNTER);
	MLX5_SET(query_vport_counter_in, in, op_mod, 0);
	MLX5_SET(query_vport_counter_in, in, other_vport, 0);

	memset(out, 0, outlen);

	/* get number of out-of-buffer drops first */
	if (mlx5_vport_query_out_of_rx_buffer(mdev, priv->counter_set_id,
	    &rx_out_of_buffer))
		goto free_out;

	/* accumulate difference into a 64-bit counter */
	s->rx_out_of_buffer += (u64)(u32)(rx_out_of_buffer - s->rx_out_of_buffer_prev);
	s->rx_out_of_buffer_prev = rx_out_of_buffer;

	/* get port statistics */
	if (mlx5_cmd_exec(mdev, in, sizeof(in), out, outlen))
		goto free_out;

#define	MLX5_GET_CTR(out, x) \
	MLX5_GET64(query_vport_counter_out, out, x)

	s->rx_error_packets =
	    MLX5_GET_CTR(out, received_errors.packets);
	s->rx_error_bytes =
	    MLX5_GET_CTR(out, received_errors.octets);
	s->tx_error_packets =
	    MLX5_GET_CTR(out, transmit_errors.packets);
	s->tx_error_bytes =
	    MLX5_GET_CTR(out, transmit_errors.octets);

	s->rx_unicast_packets =
	    MLX5_GET_CTR(out, received_eth_unicast.packets);
	s->rx_unicast_bytes =
	    MLX5_GET_CTR(out, received_eth_unicast.octets);
	s->tx_unicast_packets =
	    MLX5_GET_CTR(out, transmitted_eth_unicast.packets);
	s->tx_unicast_bytes =
	    MLX5_GET_CTR(out, transmitted_eth_unicast.octets);

	s->rx_multicast_packets =
	    MLX5_GET_CTR(out, received_eth_multicast.packets);
	s->rx_multicast_bytes =
	    MLX5_GET_CTR(out, received_eth_multicast.octets);
	s->tx_multicast_packets =
	    MLX5_GET_CTR(out, transmitted_eth_multicast.packets);
	s->tx_multicast_bytes =
	    MLX5_GET_CTR(out, transmitted_eth_multicast.octets);

	s->rx_broadcast_packets =
	    MLX5_GET_CTR(out, received_eth_broadcast.packets);
	s->rx_broadcast_bytes =
	    MLX5_GET_CTR(out, received_eth_broadcast.octets);
	s->tx_broadcast_packets =
	    MLX5_GET_CTR(out, transmitted_eth_broadcast.packets);
	s->tx_broadcast_bytes =
	    MLX5_GET_CTR(out, transmitted_eth_broadcast.octets);

	s->rx_packets =
	    s->rx_unicast_packets +
	    s->rx_multicast_packets +
	    s->rx_broadcast_packets -
	    s->rx_out_of_buffer;
	s->rx_bytes =
	    s->rx_unicast_bytes +
	    s->rx_multicast_bytes +
	    s->rx_broadcast_bytes;
	s->tx_packets =
	    s->tx_unicast_packets +
	    s->tx_multicast_packets +
	    s->tx_broadcast_packets;
	s->tx_bytes =
	    s->tx_unicast_bytes +
	    s->tx_multicast_bytes +
	    s->tx_broadcast_bytes;

	/* Update calculated offload counters */
	s->tx_csum_offload = s->tx_packets - tx_offload_none;
	s->rx_csum_good = s->rx_packets - s->rx_csum_none;

	/* Update per port counters */
	mlx5e_update_pport_counters(priv);

#if (__FreeBSD_version < 1100000)
	/* no get_counters interface in fbsd 10 */
	ifp->if_ipackets = s->rx_packets;
	ifp->if_ierrors = s->rx_error_packets;
	ifp->if_iqdrops = s->rx_out_of_buffer;
	ifp->if_opackets = s->tx_packets;
	ifp->if_oerrors = s->tx_error_packets;
	ifp->if_snd.ifq_drops = s->tx_queue_dropped;
	ifp->if_ibytes = s->rx_bytes;
	ifp->if_obytes = s->tx_bytes;
#endif

free_out:
	kvfree(out);
	PRIV_UNLOCK(priv);
}

static void
mlx5e_update_stats(void *arg)
{
	struct mlx5e_priv *priv = arg;

	schedule_work(&priv->update_stats_work);

	callout_reset(&priv->watchdog, hz, &mlx5e_update_stats, priv);
}

static void
mlx5e_async_event_sub(struct mlx5e_priv *priv,
    enum mlx5_dev_event event)
{
	switch (event) {
	case MLX5_DEV_EVENT_PORT_UP:
	case MLX5_DEV_EVENT_PORT_DOWN:
		schedule_work(&priv->update_carrier_work);
		break;

	default:
		break;
	}
}

static void
mlx5e_async_event(struct mlx5_core_dev *mdev, void *vpriv,
    enum mlx5_dev_event event, unsigned long param)
{
	struct mlx5e_priv *priv = vpriv;

	mtx_lock(&priv->async_events_mtx);
	if (test_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state))
		mlx5e_async_event_sub(priv, event);
	mtx_unlock(&priv->async_events_mtx);
}

static void
mlx5e_enable_async_events(struct mlx5e_priv *priv)
{
	set_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state);
}

static void
mlx5e_disable_async_events(struct mlx5e_priv *priv)
{
	mtx_lock(&priv->async_events_mtx);
	clear_bit(MLX5E_STATE_ASYNC_EVENTS_ENABLE, &priv->state);
	mtx_unlock(&priv->async_events_mtx);
}

static const char *mlx5e_rq_stats_desc[] = {
	MLX5E_RQ_STATS(MLX5E_STATS_DESC)
};

static int
mlx5e_create_rq(struct mlx5e_channel *c,
    struct mlx5e_rq_param *param,
    struct mlx5e_rq *rq)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	char buffer[16];
	void *rqc = param->rqc;
	void *rqc_wq = MLX5_ADDR_OF(rqc, rqc, wq);
	int wq_sz;
	int err;
	int i;

	/* Create DMA descriptor TAG */
	if ((err = -bus_dma_tag_create(
	    bus_get_dma_tag(mdev->pdev->dev.bsddev),
	    1,				/* any alignment */
	    0,				/* no boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MJUM16BYTES,		/* maxsize */
	    1,				/* nsegments */
	    MJUM16BYTES,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &rq->dma_tag)))
		goto done;

	err = mlx5_wq_ll_create(mdev, &param->wq, rqc_wq, &rq->wq,
	    &rq->wq_ctrl);
	if (err)
		goto err_free_dma_tag;

	rq->wq.db = &rq->wq.db[MLX5_RCV_DBR];

	if (priv->params.hw_lro_en) {
		rq->wqe_sz = priv->params.lro_wqe_sz;
	} else {
		rq->wqe_sz = MLX5E_SW2MB_MTU(priv->ifp->if_mtu);
	}
	if (rq->wqe_sz > MJUM16BYTES) {
		err = -ENOMEM;
		goto err_rq_wq_destroy;
	} else if (rq->wqe_sz > MJUM9BYTES) {
		rq->wqe_sz = MJUM16BYTES;
	} else if (rq->wqe_sz > MJUMPAGESIZE) {
		rq->wqe_sz = MJUM9BYTES;
	} else if (rq->wqe_sz > MCLBYTES) {
		rq->wqe_sz = MJUMPAGESIZE;
	} else {
		rq->wqe_sz = MCLBYTES;
	}

	wq_sz = mlx5_wq_ll_get_size(&rq->wq);
	rq->mbuf = malloc(wq_sz * sizeof(rq->mbuf[0]), M_MLX5EN, M_WAITOK | M_ZERO);
	if (rq->mbuf == NULL) {
		err = -ENOMEM;
		goto err_rq_wq_destroy;
	}
	for (i = 0; i != wq_sz; i++) {
		struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(&rq->wq, i);
		uint32_t byte_count = rq->wqe_sz - MLX5E_NET_IP_ALIGN;

		err = -bus_dmamap_create(rq->dma_tag, 0, &rq->mbuf[i].dma_map);
		if (err != 0) {
			while (i--)
				bus_dmamap_destroy(rq->dma_tag, rq->mbuf[i].dma_map);
			goto err_rq_mbuf_free;
		}
		wqe->data.lkey = c->mkey_be;
		wqe->data.byte_count = cpu_to_be32(byte_count | MLX5_HW_START_PADDING);
	}

	rq->pdev = c->pdev;
	rq->ifp = c->ifp;
	rq->channel = c;
	rq->ix = c->ix;

	snprintf(buffer, sizeof(buffer), "rxstat%d", c->ix);
	mlx5e_create_stats(&rq->stats.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    buffer, mlx5e_rq_stats_desc, MLX5E_RQ_STATS_NUM,
	    rq->stats.arg);

#ifdef HAVE_TURBO_LRO
	if (tcp_tlro_init(&rq->lro, c->ifp, MLX5E_BUDGET_MAX) != 0)
		rq->lro.mbuf = NULL;
#else
	if (tcp_lro_init(&rq->lro))
		rq->lro.lro_cnt = 0;
	else
		rq->lro.ifp = c->ifp;
#endif
	return (0);

err_rq_mbuf_free:
	free(rq->mbuf, M_MLX5EN);
err_rq_wq_destroy:
	mlx5_wq_destroy(&rq->wq_ctrl);
err_free_dma_tag:
	bus_dma_tag_destroy(rq->dma_tag);
done:
	return (err);
}

static void
mlx5e_destroy_rq(struct mlx5e_rq *rq)
{
	int wq_sz;
	int i;

	/* destroy all sysctl nodes */
	sysctl_ctx_free(&rq->stats.ctx);

	/* free leftover LRO packets, if any */
#ifdef HAVE_TURBO_LRO
	tcp_tlro_free(&rq->lro);
#else
	tcp_lro_free(&rq->lro);
#endif
	wq_sz = mlx5_wq_ll_get_size(&rq->wq);
	for (i = 0; i != wq_sz; i++) {
		if (rq->mbuf[i].mbuf != NULL) {
			bus_dmamap_unload(rq->dma_tag,
			    rq->mbuf[i].dma_map);
			m_freem(rq->mbuf[i].mbuf);
		}
		bus_dmamap_destroy(rq->dma_tag, rq->mbuf[i].dma_map);
	}
	free(rq->mbuf, M_MLX5EN);
	mlx5_wq_destroy(&rq->wq_ctrl);
}

static int
mlx5e_enable_rq(struct mlx5e_rq *rq, struct mlx5e_rq_param *param)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	void *in;
	void *rqc;
	void *wq;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_rq_in) +
	    sizeof(u64) * rq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	rqc = MLX5_ADDR_OF(create_rq_in, in, ctx);
	wq = MLX5_ADDR_OF(rqc, rqc, wq);

	memcpy(rqc, param->rqc, sizeof(param->rqc));

	MLX5_SET(rqc, rqc, cqn, c->rq.cq.mcq.cqn);
	MLX5_SET(rqc, rqc, state, MLX5_RQC_STATE_RST);
	MLX5_SET(rqc, rqc, flush_in_error_en, 1);
	if (priv->counter_set_id >= 0)
		MLX5_SET(rqc, rqc, counter_set_id, priv->counter_set_id);
	MLX5_SET(wq, wq, log_wq_pg_sz, rq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(wq, wq, dbr_addr, rq->wq_ctrl.db.dma);

	mlx5_fill_page_array(&rq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(wq, wq, pas));

	err = mlx5_core_create_rq(mdev, in, inlen, &rq->rqn);

	kvfree(in);

	return (err);
}

static int
mlx5e_modify_rq(struct mlx5e_rq *rq, int curr_state, int next_state)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	void *in;
	void *rqc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(modify_rq_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	rqc = MLX5_ADDR_OF(modify_rq_in, in, ctx);

	MLX5_SET(modify_rq_in, in, rqn, rq->rqn);
	MLX5_SET(modify_rq_in, in, rq_state, curr_state);
	MLX5_SET(rqc, rqc, state, next_state);

	err = mlx5_core_modify_rq(mdev, in, inlen);

	kvfree(in);

	return (err);
}

static void
mlx5e_disable_rq(struct mlx5e_rq *rq)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	mlx5_core_destroy_rq(mdev, rq->rqn);
}

static int
mlx5e_wait_for_min_rx_wqes(struct mlx5e_rq *rq)
{
	struct mlx5e_channel *c = rq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_wq_ll *wq = &rq->wq;
	int i;

	for (i = 0; i < 1000; i++) {
		if (wq->cur_sz >= priv->params.min_rx_wqes)
			return (0);

		msleep(4);
	}
	return (-ETIMEDOUT);
}

static int
mlx5e_open_rq(struct mlx5e_channel *c,
    struct mlx5e_rq_param *param,
    struct mlx5e_rq *rq)
{
	int err;

	err = mlx5e_create_rq(c, param, rq);
	if (err)
		return (err);

	err = mlx5e_enable_rq(rq, param);
	if (err)
		goto err_destroy_rq;

	err = mlx5e_modify_rq(rq, MLX5_RQC_STATE_RST, MLX5_RQC_STATE_RDY);
	if (err)
		goto err_disable_rq;

	c->rq.enabled = 1;

	return (0);

err_disable_rq:
	mlx5e_disable_rq(rq);
err_destroy_rq:
	mlx5e_destroy_rq(rq);

	return (err);
}

static void
mlx5e_close_rq(struct mlx5e_rq *rq)
{
	rq->enabled = 0;
	mlx5e_modify_rq(rq, MLX5_RQC_STATE_RDY, MLX5_RQC_STATE_ERR);
}

static void
mlx5e_close_rq_wait(struct mlx5e_rq *rq)
{
	/* wait till RQ is empty */
	while (!mlx5_wq_ll_is_empty(&rq->wq)) {
		msleep(4);
		rq->cq.mcq.comp(&rq->cq.mcq);
	}

	mlx5e_disable_rq(rq);
	mlx5e_destroy_rq(rq);
}

static void
mlx5e_free_sq_db(struct mlx5e_sq *sq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&sq->wq);
	int x;

	for (x = 0; x != wq_sz; x++)
		bus_dmamap_destroy(sq->dma_tag, sq->mbuf[x].dma_map);
	free(sq->mbuf, M_MLX5EN);
}

static int
mlx5e_alloc_sq_db(struct mlx5e_sq *sq)
{
	int wq_sz = mlx5_wq_cyc_get_size(&sq->wq);
	int err;
	int x;

	sq->mbuf = malloc(wq_sz * sizeof(sq->mbuf[0]), M_MLX5EN, M_WAITOK | M_ZERO);
	if (sq->mbuf == NULL)
		return (-ENOMEM);

	/* Create DMA descriptor MAPs */
	for (x = 0; x != wq_sz; x++) {
		err = -bus_dmamap_create(sq->dma_tag, 0, &sq->mbuf[x].dma_map);
		if (err != 0) {
			while (x--)
				bus_dmamap_destroy(sq->dma_tag, sq->mbuf[x].dma_map);
			free(sq->mbuf, M_MLX5EN);
			return (err);
		}
	}
	return (0);
}

static const char *mlx5e_sq_stats_desc[] = {
	MLX5E_SQ_STATS(MLX5E_STATS_DESC)
};

static int
mlx5e_create_sq(struct mlx5e_channel *c,
    int tc,
    struct mlx5e_sq_param *param,
    struct mlx5e_sq *sq)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	char buffer[16];

	void *sqc = param->sqc;
	void *sqc_wq = MLX5_ADDR_OF(sqc, sqc, wq);
#ifdef RSS
	cpuset_t cpu_mask;
	int cpu_id;
#endif
	int err;

	/* Create DMA descriptor TAG */
	if ((err = -bus_dma_tag_create(
	    bus_get_dma_tag(mdev->pdev->dev.bsddev),
	    1,				/* any alignment */
	    0,				/* no boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MLX5E_MAX_TX_PAYLOAD_SIZE,	/* maxsize */
	    MLX5E_MAX_TX_MBUF_FRAGS,	/* nsegments */
	    MLX5E_MAX_TX_MBUF_SIZE,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sq->dma_tag)))
		goto done;

	err = mlx5_alloc_map_uar(mdev, &sq->uar);
	if (err)
		goto err_free_dma_tag;

	err = mlx5_wq_cyc_create(mdev, &param->wq, sqc_wq, &sq->wq,
	    &sq->wq_ctrl);
	if (err)
		goto err_unmap_free_uar;

	sq->wq.db = &sq->wq.db[MLX5_SND_DBR];
	sq->uar_map = sq->uar.map;
	sq->uar_bf_map = sq->uar.bf_map;
	sq->bf_buf_size = (1 << MLX5_CAP_GEN(mdev, log_bf_reg_size)) / 2;

	err = mlx5e_alloc_sq_db(sq);
	if (err)
		goto err_sq_wq_destroy;

	sq->pdev = c->pdev;
	sq->mkey_be = c->mkey_be;
	sq->channel = c;
	sq->tc = tc;

	sq->br = buf_ring_alloc(MLX5E_SQ_TX_QUEUE_SIZE, M_MLX5EN,
	    M_WAITOK, &sq->lock);
	if (sq->br == NULL) {
		if_printf(c->ifp, "%s: Failed allocating sq drbr buffer\n",
		    __func__);
		err = -ENOMEM;
		goto err_free_sq_db;
	}

	sq->sq_tq = taskqueue_create_fast("mlx5e_que", M_WAITOK,
	    taskqueue_thread_enqueue, &sq->sq_tq);
	if (sq->sq_tq == NULL) {
		if_printf(c->ifp, "%s: Failed allocating taskqueue\n",
		    __func__);
		err = -ENOMEM;
		goto err_free_drbr;
	}

	TASK_INIT(&sq->sq_task, 0, mlx5e_tx_que, sq);
#ifdef RSS
	cpu_id = rss_getcpu(c->ix % rss_getnumbuckets());
	CPU_SETOF(cpu_id, &cpu_mask);
	taskqueue_start_threads_cpuset(&sq->sq_tq, 1, PI_NET, &cpu_mask,
	    "%s TX SQ%d.%d CPU%d", c->ifp->if_xname, c->ix, tc, cpu_id);
#else
	taskqueue_start_threads(&sq->sq_tq, 1, PI_NET,
	    "%s TX SQ%d.%d", c->ifp->if_xname, c->ix, tc);
#endif
	snprintf(buffer, sizeof(buffer), "txstat%dtc%d", c->ix, tc);
	mlx5e_create_stats(&sq->stats.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    buffer, mlx5e_sq_stats_desc, MLX5E_SQ_STATS_NUM,
	    sq->stats.arg);

	return (0);

err_free_drbr:
	buf_ring_free(sq->br, M_MLX5EN);
err_free_sq_db:
	mlx5e_free_sq_db(sq);
err_sq_wq_destroy:
	mlx5_wq_destroy(&sq->wq_ctrl);

err_unmap_free_uar:
	mlx5_unmap_free_uar(mdev, &sq->uar);

err_free_dma_tag:
	bus_dma_tag_destroy(sq->dma_tag);
done:
	return (err);
}

static void
mlx5e_destroy_sq(struct mlx5e_sq *sq)
{
	struct mlx5e_channel *c = sq->channel;
	struct mlx5e_priv *priv = c->priv;

	/* destroy all sysctl nodes */
	sysctl_ctx_free(&sq->stats.ctx);

	mlx5e_free_sq_db(sq);
	mlx5_wq_destroy(&sq->wq_ctrl);
	mlx5_unmap_free_uar(priv->mdev, &sq->uar);
	taskqueue_drain(sq->sq_tq, &sq->sq_task);
	taskqueue_free(sq->sq_tq);
	buf_ring_free(sq->br, M_MLX5EN);
}

static int
mlx5e_enable_sq(struct mlx5e_sq *sq, struct mlx5e_sq_param *param)
{
	struct mlx5e_channel *c = sq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	void *in;
	void *sqc;
	void *wq;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_sq_in) +
	    sizeof(u64) * sq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	sqc = MLX5_ADDR_OF(create_sq_in, in, ctx);
	wq = MLX5_ADDR_OF(sqc, sqc, wq);

	memcpy(sqc, param->sqc, sizeof(param->sqc));

	MLX5_SET(sqc, sqc, tis_num_0, priv->tisn[sq->tc]);
	MLX5_SET(sqc, sqc, cqn, c->sq[sq->tc].cq.mcq.cqn);
	MLX5_SET(sqc, sqc, state, MLX5_SQC_STATE_RST);
	MLX5_SET(sqc, sqc, tis_lst_sz, 1);
	MLX5_SET(sqc, sqc, flush_in_error_en, 1);

	MLX5_SET(wq, wq, wq_type, MLX5_WQ_TYPE_CYCLIC);
	MLX5_SET(wq, wq, uar_page, sq->uar.index);
	MLX5_SET(wq, wq, log_wq_pg_sz, sq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(wq, wq, dbr_addr, sq->wq_ctrl.db.dma);

	mlx5_fill_page_array(&sq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(wq, wq, pas));

	err = mlx5_core_create_sq(mdev, in, inlen, &sq->sqn);

	kvfree(in);

	return (err);
}

static int
mlx5e_modify_sq(struct mlx5e_sq *sq, int curr_state, int next_state)
{
	struct mlx5e_channel *c = sq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	void *in;
	void *sqc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(modify_sq_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	sqc = MLX5_ADDR_OF(modify_sq_in, in, ctx);

	MLX5_SET(modify_sq_in, in, sqn, sq->sqn);
	MLX5_SET(modify_sq_in, in, sq_state, curr_state);
	MLX5_SET(sqc, sqc, state, next_state);

	err = mlx5_core_modify_sq(mdev, in, inlen);

	kvfree(in);

	return (err);
}

static void
mlx5e_disable_sq(struct mlx5e_sq *sq)
{
	struct mlx5e_channel *c = sq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	mlx5_core_destroy_sq(mdev, sq->sqn);
}

static int
mlx5e_open_sq(struct mlx5e_channel *c,
    int tc,
    struct mlx5e_sq_param *param,
    struct mlx5e_sq *sq)
{
	int err;

	err = mlx5e_create_sq(c, tc, param, sq);
	if (err)
		return (err);

	err = mlx5e_enable_sq(sq, param);
	if (err)
		goto err_destroy_sq;

	err = mlx5e_modify_sq(sq, MLX5_SQC_STATE_RST, MLX5_SQC_STATE_RDY);
	if (err)
		goto err_disable_sq;

	atomic_store_rel_int(&sq->queue_state, MLX5E_SQ_READY);

	return (0);

err_disable_sq:
	mlx5e_disable_sq(sq);
err_destroy_sq:
	mlx5e_destroy_sq(sq);

	return (err);
}

static void
mlx5e_sq_send_nops_locked(struct mlx5e_sq *sq, int can_sleep)
{
	/* fill up remainder with NOPs */
	while (sq->cev_counter != 0) {
		while (!mlx5e_sq_has_room_for(sq, 1)) {
			if (can_sleep != 0) {
				mtx_unlock(&sq->lock);
				msleep(4);
				mtx_lock(&sq->lock);
			} else {
				goto done;
			}
		}
		/* send a single NOP */
		mlx5e_send_nop(sq, 1);
		wmb();
	}
done:
	/* Check if we need to write the doorbell */
	if (likely(sq->doorbell.d64 != 0)) {
		mlx5e_tx_notify_hw(sq, sq->doorbell.d32, 0);
		sq->doorbell.d64 = 0;
	}
	return;
}

void
mlx5e_sq_cev_timeout(void *arg)
{
	struct mlx5e_sq *sq = arg;

	mtx_assert(&sq->lock, MA_OWNED);

	/* check next state */
	switch (sq->cev_next_state) {
	case MLX5E_CEV_STATE_SEND_NOPS:
		/* fill TX ring with NOPs, if any */
		mlx5e_sq_send_nops_locked(sq, 0);

		/* check if completed */
		if (sq->cev_counter == 0) {
			sq->cev_next_state = MLX5E_CEV_STATE_INITIAL;
			return;
		}
		break;
	default:
		/* send NOPs on next timeout */
		sq->cev_next_state = MLX5E_CEV_STATE_SEND_NOPS;
		break;
	}

	/* restart timer */
	callout_reset_curcpu(&sq->cev_callout, hz, mlx5e_sq_cev_timeout, sq);
}

static void
mlx5e_close_sq_wait(struct mlx5e_sq *sq)
{

	mtx_lock(&sq->lock);
	/* teardown event factor timer, if any */
	sq->cev_next_state = MLX5E_CEV_STATE_HOLD_NOPS;
	callout_stop(&sq->cev_callout);

	/* send dummy NOPs in order to flush the transmit ring */
	mlx5e_sq_send_nops_locked(sq, 1);
	mtx_unlock(&sq->lock);

	/* make sure it is safe to free the callout */
	callout_drain(&sq->cev_callout);

	/* error out remaining requests */
	mlx5e_modify_sq(sq, MLX5_SQC_STATE_RDY, MLX5_SQC_STATE_ERR);

	/* wait till SQ is empty */
	mtx_lock(&sq->lock);
	while (sq->cc != sq->pc) {
		mtx_unlock(&sq->lock);
		msleep(4);
		sq->cq.mcq.comp(&sq->cq.mcq);
		mtx_lock(&sq->lock);
	}
	mtx_unlock(&sq->lock);

	mlx5e_disable_sq(sq);
	mlx5e_destroy_sq(sq);
}

static int
mlx5e_create_cq(struct mlx5e_channel *c,
    struct mlx5e_cq_param *param,
    struct mlx5e_cq *cq,
    mlx5e_cq_comp_t *comp)
{
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_core_cq *mcq = &cq->mcq;
	int eqn_not_used;
	int irqn;
	int err;
	u32 i;

	param->wq.buf_numa_node = 0;
	param->wq.db_numa_node = 0;
	param->eq_ix = c->ix;

	err = mlx5_cqwq_create(mdev, &param->wq, param->cqc, &cq->wq,
	    &cq->wq_ctrl);
	if (err)
		return (err);

	mlx5_vector2eqn(mdev, param->eq_ix, &eqn_not_used, &irqn);

	mcq->cqe_sz = 64;
	mcq->set_ci_db = cq->wq_ctrl.db.db;
	mcq->arm_db = cq->wq_ctrl.db.db + 1;
	*mcq->set_ci_db = 0;
	*mcq->arm_db = 0;
	mcq->vector = param->eq_ix;
	mcq->comp = comp;
	mcq->event = mlx5e_cq_error_event;
	mcq->irqn = irqn;
	mcq->uar = &priv->cq_uar;

	for (i = 0; i < mlx5_cqwq_get_size(&cq->wq); i++) {
		struct mlx5_cqe64 *cqe = mlx5_cqwq_get_wqe(&cq->wq, i);

		cqe->op_own = 0xf1;
	}

	cq->channel = c;

	return (0);
}

static void
mlx5e_destroy_cq(struct mlx5e_cq *cq)
{
	mlx5_wq_destroy(&cq->wq_ctrl);
}

static int
mlx5e_enable_cq(struct mlx5e_cq *cq, struct mlx5e_cq_param *param,
    u8 moderation_mode)
{
	struct mlx5e_channel *c = cq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_core_cq *mcq = &cq->mcq;
	void *in;
	void *cqc;
	int inlen;
	int irqn_not_used;
	int eqn;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_cq_in) +
	    sizeof(u64) * cq->wq_ctrl.buf.npages;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);

	cqc = MLX5_ADDR_OF(create_cq_in, in, cq_context);

	memcpy(cqc, param->cqc, sizeof(param->cqc));

	mlx5_fill_page_array(&cq->wq_ctrl.buf,
	    (__be64 *) MLX5_ADDR_OF(create_cq_in, in, pas));

	mlx5_vector2eqn(mdev, param->eq_ix, &eqn, &irqn_not_used);

	MLX5_SET(cqc, cqc, cq_period_mode, moderation_mode);
	MLX5_SET(cqc, cqc, c_eqn, eqn);
	MLX5_SET(cqc, cqc, uar_page, mcq->uar->index);
	MLX5_SET(cqc, cqc, log_page_size, cq->wq_ctrl.buf.page_shift -
	    PAGE_SHIFT);
	MLX5_SET64(cqc, cqc, dbr_addr, cq->wq_ctrl.db.dma);

	err = mlx5_core_create_cq(mdev, mcq, in, inlen);

	kvfree(in);

	if (err)
		return (err);

	mlx5e_cq_arm(cq);

	return (0);
}

static void
mlx5e_disable_cq(struct mlx5e_cq *cq)
{
	struct mlx5e_channel *c = cq->channel;
	struct mlx5e_priv *priv = c->priv;
	struct mlx5_core_dev *mdev = priv->mdev;

	mlx5_core_destroy_cq(mdev, &cq->mcq);
}

static int
mlx5e_open_cq(struct mlx5e_channel *c,
    struct mlx5e_cq_param *param,
    struct mlx5e_cq *cq,
    mlx5e_cq_comp_t *comp,
    u8 moderation_mode)
{
	int err;

	err = mlx5e_create_cq(c, param, cq, comp);
	if (err)
		return (err);

	err = mlx5e_enable_cq(cq, param, moderation_mode);
	if (err)
		goto err_destroy_cq;

	return (0);

err_destroy_cq:
	mlx5e_destroy_cq(cq);

	return (err);
}

static void
mlx5e_close_cq(struct mlx5e_cq *cq)
{
	mlx5e_disable_cq(cq);
	mlx5e_destroy_cq(cq);
}

static int
mlx5e_open_tx_cqs(struct mlx5e_channel *c,
    struct mlx5e_channel_param *cparam)
{
	u8 tx_moderation_mode;
	int err;
	int tc;

	switch (c->priv->params.tx_cq_moderation_mode) {
	case 0:
		tx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
		break;
	default:
		if (MLX5_CAP_GEN(c->priv->mdev, cq_period_start_from_cqe))
			tx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_CQE;
		else
			tx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
		break;
	}
	for (tc = 0; tc < c->num_tc; tc++) {
		/* open completion queue */
		err = mlx5e_open_cq(c, &cparam->tx_cq, &c->sq[tc].cq,
		    &mlx5e_tx_cq_comp, tx_moderation_mode);
		if (err)
			goto err_close_tx_cqs;
	}
	return (0);

err_close_tx_cqs:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_cq(&c->sq[tc].cq);

	return (err);
}

static void
mlx5e_close_tx_cqs(struct mlx5e_channel *c)
{
	int tc;

	for (tc = 0; tc < c->num_tc; tc++)
		mlx5e_close_cq(&c->sq[tc].cq);
}

static int
mlx5e_open_sqs(struct mlx5e_channel *c,
    struct mlx5e_channel_param *cparam)
{
	int err;
	int tc;

	for (tc = 0; tc < c->num_tc; tc++) {
		err = mlx5e_open_sq(c, tc, &cparam->sq, &c->sq[tc]);
		if (err)
			goto err_close_sqs;
	}

	return (0);

err_close_sqs:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_sq_wait(&c->sq[tc]);

	return (err);
}

static void
mlx5e_close_sqs_wait(struct mlx5e_channel *c)
{
	int tc;

	for (tc = 0; tc < c->num_tc; tc++)
		mlx5e_close_sq_wait(&c->sq[tc]);
}

static void
mlx5e_chan_mtx_init(struct mlx5e_channel *c)
{
	int tc;

	mtx_init(&c->rq.mtx, "mlx5rx", MTX_NETWORK_LOCK, MTX_DEF);

	for (tc = 0; tc < c->num_tc; tc++) {
		struct mlx5e_sq *sq = c->sq + tc;

		mtx_init(&sq->lock, "mlx5tx", MTX_NETWORK_LOCK, MTX_DEF);
		mtx_init(&sq->comp_lock, "mlx5comp", MTX_NETWORK_LOCK,
		    MTX_DEF);

		callout_init_mtx(&sq->cev_callout, &sq->lock, 0);

		sq->cev_factor = c->priv->params_ethtool.tx_completion_fact;

		/* ensure the TX completion event factor is not zero */
		if (sq->cev_factor == 0)
			sq->cev_factor = 1;
	}
}

static void
mlx5e_chan_mtx_destroy(struct mlx5e_channel *c)
{
	int tc;

	mtx_destroy(&c->rq.mtx);

	for (tc = 0; tc < c->num_tc; tc++) {
		mtx_destroy(&c->sq[tc].lock);
		mtx_destroy(&c->sq[tc].comp_lock);
	}
}

static int
mlx5e_open_channel(struct mlx5e_priv *priv, int ix,
    struct mlx5e_channel_param *cparam,
    struct mlx5e_channel *volatile *cp)
{
	struct mlx5e_channel *c;
	u8 rx_moderation_mode;
	int err;

	c = malloc(sizeof(*c), M_MLX5EN, M_WAITOK | M_ZERO);
	if (c == NULL)
		return (-ENOMEM);

	c->priv = priv;
	c->ix = ix;
	c->cpu = 0;
	c->pdev = &priv->mdev->pdev->dev;
	c->ifp = priv->ifp;
	c->mkey_be = cpu_to_be32(priv->mr.key);
	c->num_tc = priv->num_tc;

	/* init mutexes */
	mlx5e_chan_mtx_init(c);

	/* open transmit completion queue */
	err = mlx5e_open_tx_cqs(c, cparam);
	if (err)
		goto err_free;

	switch (priv->params.rx_cq_moderation_mode) {
	case 0:
		rx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
		break;
	default:
		if (MLX5_CAP_GEN(priv->mdev, cq_period_start_from_cqe))
			rx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_CQE;
		else
			rx_moderation_mode = MLX5_CQ_PERIOD_MODE_START_FROM_EQE;
		break;
	}

	/* open receive completion queue */
	err = mlx5e_open_cq(c, &cparam->rx_cq, &c->rq.cq,
	    &mlx5e_rx_cq_comp, rx_moderation_mode);
	if (err)
		goto err_close_tx_cqs;

	err = mlx5e_open_sqs(c, cparam);
	if (err)
		goto err_close_rx_cq;

	err = mlx5e_open_rq(c, &cparam->rq, &c->rq);
	if (err)
		goto err_close_sqs;

	/* store channel pointer */
	*cp = c;

	/* poll receive queue initially */
	c->rq.cq.mcq.comp(&c->rq.cq.mcq);

	return (0);

err_close_sqs:
	mlx5e_close_sqs_wait(c);

err_close_rx_cq:
	mlx5e_close_cq(&c->rq.cq);

err_close_tx_cqs:
	mlx5e_close_tx_cqs(c);

err_free:
	/* destroy mutexes */
	mlx5e_chan_mtx_destroy(c);
	free(c, M_MLX5EN);
	return (err);
}

static void
mlx5e_close_channel(struct mlx5e_channel *volatile *pp)
{
	struct mlx5e_channel *c = *pp;

	/* check if channel is already closed */
	if (c == NULL)
		return;
	mlx5e_close_rq(&c->rq);
}

static void
mlx5e_close_channel_wait(struct mlx5e_channel *volatile *pp)
{
	struct mlx5e_channel *c = *pp;

	/* check if channel is already closed */
	if (c == NULL)
		return;
	/* ensure channel pointer is no longer used */
	*pp = NULL;

	mlx5e_close_rq_wait(&c->rq);
	mlx5e_close_sqs_wait(c);
	mlx5e_close_cq(&c->rq.cq);
	mlx5e_close_tx_cqs(c);
	/* destroy mutexes */
	mlx5e_chan_mtx_destroy(c);
	free(c, M_MLX5EN);
}

static void
mlx5e_build_rq_param(struct mlx5e_priv *priv,
    struct mlx5e_rq_param *param)
{
	void *rqc = param->rqc;
	void *wq = MLX5_ADDR_OF(rqc, rqc, wq);

	MLX5_SET(wq, wq, wq_type, MLX5_WQ_TYPE_LINKED_LIST);
	MLX5_SET(wq, wq, end_padding_mode, MLX5_WQ_END_PAD_MODE_ALIGN);
	MLX5_SET(wq, wq, log_wq_stride, ilog2(sizeof(struct mlx5e_rx_wqe)));
	MLX5_SET(wq, wq, log_wq_sz, priv->params.log_rq_size);
	MLX5_SET(wq, wq, pd, priv->pdn);

	param->wq.buf_numa_node = 0;
	param->wq.db_numa_node = 0;
	param->wq.linear = 1;
}

static void
mlx5e_build_sq_param(struct mlx5e_priv *priv,
    struct mlx5e_sq_param *param)
{
	void *sqc = param->sqc;
	void *wq = MLX5_ADDR_OF(sqc, sqc, wq);

	MLX5_SET(wq, wq, log_wq_sz, priv->params.log_sq_size);
	MLX5_SET(wq, wq, log_wq_stride, ilog2(MLX5_SEND_WQE_BB));
	MLX5_SET(wq, wq, pd, priv->pdn);

	param->wq.buf_numa_node = 0;
	param->wq.db_numa_node = 0;
	param->wq.linear = 1;
}

static void
mlx5e_build_common_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	void *cqc = param->cqc;

	MLX5_SET(cqc, cqc, uar_page, priv->cq_uar.index);
}

static void
mlx5e_build_rx_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	void *cqc = param->cqc;


	/*
	 * TODO The sysctl to control on/off is a bool value for now, which means
	 * we only support CSUM, once HASH is implemnted we'll need to address that.
	 */
	if (priv->params.cqe_zipping_en) {
		MLX5_SET(cqc, cqc, mini_cqe_res_format, MLX5_CQE_FORMAT_CSUM);
		MLX5_SET(cqc, cqc, cqe_compression_en, 1);
	}

	MLX5_SET(cqc, cqc, log_cq_size, priv->params.log_rq_size);
	MLX5_SET(cqc, cqc, cq_period, priv->params.rx_cq_moderation_usec);
	MLX5_SET(cqc, cqc, cq_max_count, priv->params.rx_cq_moderation_pkts);

	mlx5e_build_common_cq_param(priv, param);
}

static void
mlx5e_build_tx_cq_param(struct mlx5e_priv *priv,
    struct mlx5e_cq_param *param)
{
	void *cqc = param->cqc;

	MLX5_SET(cqc, cqc, log_cq_size, priv->params.log_sq_size);
	MLX5_SET(cqc, cqc, cq_period, priv->params.tx_cq_moderation_usec);
	MLX5_SET(cqc, cqc, cq_max_count, priv->params.tx_cq_moderation_pkts);

	mlx5e_build_common_cq_param(priv, param);
}

static void
mlx5e_build_channel_param(struct mlx5e_priv *priv,
    struct mlx5e_channel_param *cparam)
{
	memset(cparam, 0, sizeof(*cparam));

	mlx5e_build_rq_param(priv, &cparam->rq);
	mlx5e_build_sq_param(priv, &cparam->sq);
	mlx5e_build_rx_cq_param(priv, &cparam->rx_cq);
	mlx5e_build_tx_cq_param(priv, &cparam->tx_cq);
}

static int
mlx5e_open_channels(struct mlx5e_priv *priv)
{
	struct mlx5e_channel_param cparam;
	void *ptr;
	int err;
	int i;
	int j;

	priv->channel = malloc(priv->params.num_channels *
	    sizeof(struct mlx5e_channel *), M_MLX5EN, M_WAITOK | M_ZERO);
	if (priv->channel == NULL)
		return (-ENOMEM);

	mlx5e_build_channel_param(priv, &cparam);
	for (i = 0; i < priv->params.num_channels; i++) {
		err = mlx5e_open_channel(priv, i, &cparam, &priv->channel[i]);
		if (err)
			goto err_close_channels;
	}

	for (j = 0; j < priv->params.num_channels; j++) {
		err = mlx5e_wait_for_min_rx_wqes(&priv->channel[j]->rq);
		if (err)
			goto err_close_channels;
	}

	return (0);

err_close_channels:
	for (i--; i >= 0; i--) {
		mlx5e_close_channel(&priv->channel[i]);
		mlx5e_close_channel_wait(&priv->channel[i]);
	}

	/* remove "volatile" attribute from "channel" pointer */
	ptr = __DECONST(void *, priv->channel);
	priv->channel = NULL;

	free(ptr, M_MLX5EN);

	return (err);
}

static void
mlx5e_close_channels(struct mlx5e_priv *priv)
{
	void *ptr;
	int i;

	if (priv->channel == NULL)
		return;

	for (i = 0; i < priv->params.num_channels; i++)
		mlx5e_close_channel(&priv->channel[i]);
	for (i = 0; i < priv->params.num_channels; i++)
		mlx5e_close_channel_wait(&priv->channel[i]);

	/* remove "volatile" attribute from "channel" pointer */
	ptr = __DECONST(void *, priv->channel);
	priv->channel = NULL;

	free(ptr, M_MLX5EN);
}

static int
mlx5e_refresh_sq_params(struct mlx5e_priv *priv, struct mlx5e_sq *sq)
{
	return (mlx5_core_modify_cq_moderation(priv->mdev, &sq->cq.mcq,
	    priv->params.tx_cq_moderation_usec,
	    priv->params.tx_cq_moderation_pkts));
}

static int
mlx5e_refresh_rq_params(struct mlx5e_priv *priv, struct mlx5e_rq *rq)
{
	return (mlx5_core_modify_cq_moderation(priv->mdev, &rq->cq.mcq,
	    priv->params.rx_cq_moderation_usec,
	    priv->params.rx_cq_moderation_pkts));
}

static int
mlx5e_refresh_channel_params_sub(struct mlx5e_priv *priv, struct mlx5e_channel *c)
{
	int err;
	int i;

	if (c == NULL)
		return (EINVAL);

	err = mlx5e_refresh_rq_params(priv, &c->rq);
	if (err)
		goto done;

	for (i = 0; i != c->num_tc; i++) {
		err = mlx5e_refresh_sq_params(priv, &c->sq[i]);
		if (err)
			goto done;
	}
done:
	return (err);
}

int
mlx5e_refresh_channel_params(struct mlx5e_priv *priv)
{
	int i;

	if (priv->channel == NULL)
		return (EINVAL);

	for (i = 0; i < priv->params.num_channels; i++) {
		int err;

		err = mlx5e_refresh_channel_params_sub(priv, priv->channel[i]);
		if (err)
			return (err);
	}
	return (0);
}

static int
mlx5e_open_tis(struct mlx5e_priv *priv, int tc)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 in[MLX5_ST_SZ_DW(create_tis_in)];
	void *tisc = MLX5_ADDR_OF(create_tis_in, in, ctx);

	memset(in, 0, sizeof(in));

	MLX5_SET(tisc, tisc, prio, tc);
	MLX5_SET(tisc, tisc, transport_domain, priv->tdn);

	return (mlx5_core_create_tis(mdev, in, sizeof(in), &priv->tisn[tc]));
}

static void
mlx5e_close_tis(struct mlx5e_priv *priv, int tc)
{
	mlx5_core_destroy_tis(priv->mdev, priv->tisn[tc]);
}

static int
mlx5e_open_tises(struct mlx5e_priv *priv)
{
	int num_tc = priv->num_tc;
	int err;
	int tc;

	for (tc = 0; tc < num_tc; tc++) {
		err = mlx5e_open_tis(priv, tc);
		if (err)
			goto err_close_tises;
	}

	return (0);

err_close_tises:
	for (tc--; tc >= 0; tc--)
		mlx5e_close_tis(priv, tc);

	return (err);
}

static void
mlx5e_close_tises(struct mlx5e_priv *priv)
{
	int num_tc = priv->num_tc;
	int tc;

	for (tc = 0; tc < num_tc; tc++)
		mlx5e_close_tis(priv, tc);
}

static int
mlx5e_open_rqt(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 *in;
	u32 out[MLX5_ST_SZ_DW(create_rqt_out)];
	void *rqtc;
	int inlen;
	int err;
	int sz;
	int i;

	sz = 1 << priv->params.rx_hash_log_tbl_sz;

	inlen = MLX5_ST_SZ_BYTES(create_rqt_in) + sizeof(u32) * sz;
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);
	rqtc = MLX5_ADDR_OF(create_rqt_in, in, rqt_context);

	MLX5_SET(rqtc, rqtc, rqt_actual_size, sz);
	MLX5_SET(rqtc, rqtc, rqt_max_size, sz);

	for (i = 0; i < sz; i++) {
		int ix;
#ifdef RSS
		ix = rss_get_indirection_to_bucket(i);
#else
		ix = i;
#endif
		/* ensure we don't overflow */
		ix %= priv->params.num_channels;
		MLX5_SET(rqtc, rqtc, rq_num[i], priv->channel[ix]->rq.rqn);
	}

	MLX5_SET(create_rqt_in, in, opcode, MLX5_CMD_OP_CREATE_RQT);

	memset(out, 0, sizeof(out));
	err = mlx5_cmd_exec_check_status(mdev, in, inlen, out, sizeof(out));
	if (!err)
		priv->rqtn = MLX5_GET(create_rqt_out, out, rqtn);

	kvfree(in);

	return (err);
}

static void
mlx5e_close_rqt(struct mlx5e_priv *priv)
{
	u32 in[MLX5_ST_SZ_DW(destroy_rqt_in)];
	u32 out[MLX5_ST_SZ_DW(destroy_rqt_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(destroy_rqt_in, in, opcode, MLX5_CMD_OP_DESTROY_RQT);
	MLX5_SET(destroy_rqt_in, in, rqtn, priv->rqtn);

	mlx5_cmd_exec_check_status(priv->mdev, in, sizeof(in), out,
	    sizeof(out));
}

static void
mlx5e_build_tir_ctx(struct mlx5e_priv *priv, u32 * tirc, int tt)
{
	void *hfso = MLX5_ADDR_OF(tirc, tirc, rx_hash_field_selector_outer);
	__be32 *hkey;

	MLX5_SET(tirc, tirc, transport_domain, priv->tdn);

#define	ROUGH_MAX_L2_L3_HDR_SZ 256

#define	MLX5_HASH_IP     (MLX5_HASH_FIELD_SEL_SRC_IP   |\
			  MLX5_HASH_FIELD_SEL_DST_IP)

#define	MLX5_HASH_ALL    (MLX5_HASH_FIELD_SEL_SRC_IP   |\
			  MLX5_HASH_FIELD_SEL_DST_IP   |\
			  MLX5_HASH_FIELD_SEL_L4_SPORT |\
			  MLX5_HASH_FIELD_SEL_L4_DPORT)

#define	MLX5_HASH_IP_IPSEC_SPI	(MLX5_HASH_FIELD_SEL_SRC_IP   |\
				 MLX5_HASH_FIELD_SEL_DST_IP   |\
				 MLX5_HASH_FIELD_SEL_IPSEC_SPI)

	if (priv->params.hw_lro_en) {
		MLX5_SET(tirc, tirc, lro_enable_mask,
		    MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO |
		    MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO);
		MLX5_SET(tirc, tirc, lro_max_msg_sz,
		    (priv->params.lro_wqe_sz -
		    ROUGH_MAX_L2_L3_HDR_SZ) >> 8);
		/* TODO: add the option to choose timer value dynamically */
		MLX5_SET(tirc, tirc, lro_timeout_period_usecs,
		    MLX5_CAP_ETH(priv->mdev,
		    lro_timer_supported_periods[2]));
	}

	/* setup parameters for hashing TIR type, if any */
	switch (tt) {
	case MLX5E_TT_ANY:
		MLX5_SET(tirc, tirc, disp_type,
		    MLX5_TIRC_DISP_TYPE_DIRECT);
		MLX5_SET(tirc, tirc, inline_rqn,
		    priv->channel[0]->rq.rqn);
		break;
	default:
		MLX5_SET(tirc, tirc, disp_type,
		    MLX5_TIRC_DISP_TYPE_INDIRECT);
		MLX5_SET(tirc, tirc, indirect_table,
		    priv->rqtn);
		MLX5_SET(tirc, tirc, rx_hash_fn,
		    MLX5_TIRC_RX_HASH_FN_HASH_TOEPLITZ);
		hkey = (__be32 *) MLX5_ADDR_OF(tirc, tirc, rx_hash_toeplitz_key);
#ifdef RSS
		/*
		 * The FreeBSD RSS implementation does currently not
		 * support symmetric Toeplitz hashes:
		 */
		MLX5_SET(tirc, tirc, rx_hash_symmetric, 0);
		rss_getkey((uint8_t *)hkey);
#else
		MLX5_SET(tirc, tirc, rx_hash_symmetric, 1);
		hkey[0] = cpu_to_be32(0xD181C62C);
		hkey[1] = cpu_to_be32(0xF7F4DB5B);
		hkey[2] = cpu_to_be32(0x1983A2FC);
		hkey[3] = cpu_to_be32(0x943E1ADB);
		hkey[4] = cpu_to_be32(0xD9389E6B);
		hkey[5] = cpu_to_be32(0xD1039C2C);
		hkey[6] = cpu_to_be32(0xA74499AD);
		hkey[7] = cpu_to_be32(0x593D56D9);
		hkey[8] = cpu_to_be32(0xF3253C06);
		hkey[9] = cpu_to_be32(0x2ADC1FFC);
#endif
		break;
	}

	switch (tt) {
	case MLX5E_TT_IPV4_TCP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfso, l4_prot_type,
		    MLX5_L4_PROT_TYPE_TCP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_TCP_IPV4)) {
			MLX5_SET(rx_hash_field_select, hfso, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV6_TCP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfso, l4_prot_type,
		    MLX5_L4_PROT_TYPE_TCP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_TCP_IPV6)) {
			MLX5_SET(rx_hash_field_select, hfso, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV4_UDP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfso, l4_prot_type,
		    MLX5_L4_PROT_TYPE_UDP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_UDP_IPV4)) {
			MLX5_SET(rx_hash_field_select, hfso, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV6_UDP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfso, l4_prot_type,
		    MLX5_L4_PROT_TYPE_UDP);
#ifdef RSS
		if (!(rss_gethashconfig() & RSS_HASHTYPE_RSS_UDP_IPV6)) {
			MLX5_SET(rx_hash_field_select, hfso, selected_fields,
			    MLX5_HASH_IP);
		} else
#endif
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_ALL);
		break;

	case MLX5E_TT_IPV4_IPSEC_AH:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV6_IPSEC_AH:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV4_IPSEC_ESP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV6_IPSEC_ESP:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP_IPSEC_SPI);
		break;

	case MLX5E_TT_IPV4:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV4);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP);
		break;

	case MLX5E_TT_IPV6:
		MLX5_SET(rx_hash_field_select, hfso, l3_prot_type,
		    MLX5_L3_PROT_TYPE_IPV6);
		MLX5_SET(rx_hash_field_select, hfso, selected_fields,
		    MLX5_HASH_IP);
		break;

	default:
		break;
	}
}

static int
mlx5e_open_tir(struct mlx5e_priv *priv, int tt)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	u32 *in;
	void *tirc;
	int inlen;
	int err;

	inlen = MLX5_ST_SZ_BYTES(create_tir_in);
	in = mlx5_vzalloc(inlen);
	if (in == NULL)
		return (-ENOMEM);
	tirc = MLX5_ADDR_OF(create_tir_in, in, tir_context);

	mlx5e_build_tir_ctx(priv, tirc, tt);

	err = mlx5_core_create_tir(mdev, in, inlen, &priv->tirn[tt]);

	kvfree(in);

	return (err);
}

static void
mlx5e_close_tir(struct mlx5e_priv *priv, int tt)
{
	mlx5_core_destroy_tir(priv->mdev, priv->tirn[tt]);
}

static int
mlx5e_open_tirs(struct mlx5e_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < MLX5E_NUM_TT; i++) {
		err = mlx5e_open_tir(priv, i);
		if (err)
			goto err_close_tirs;
	}

	return (0);

err_close_tirs:
	for (i--; i >= 0; i--)
		mlx5e_close_tir(priv, i);

	return (err);
}

static void
mlx5e_close_tirs(struct mlx5e_priv *priv)
{
	int i;

	for (i = 0; i < MLX5E_NUM_TT; i++)
		mlx5e_close_tir(priv, i);
}

/*
 * SW MTU does not include headers,
 * HW MTU includes all headers and checksums.
 */
static int
mlx5e_set_dev_port_mtu(struct ifnet *ifp, int sw_mtu)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	struct mlx5_core_dev *mdev = priv->mdev;
	int hw_mtu;
	int err;


	err = mlx5_set_port_mtu(mdev, MLX5E_SW2HW_MTU(sw_mtu));
	if (err) {
		if_printf(ifp, "%s: mlx5_set_port_mtu failed setting %d, err=%d\n",
		    __func__, sw_mtu, err);
		return (err);
	}
	err = mlx5_query_port_oper_mtu(mdev, &hw_mtu);
	if (!err) {
		ifp->if_mtu = MLX5E_HW2SW_MTU(hw_mtu);

		if (ifp->if_mtu != sw_mtu) {
			if_printf(ifp, "Port MTU %d is different than "
			    "ifp mtu %d\n", sw_mtu, (int)ifp->if_mtu);
		}
	} else {
		if_printf(ifp, "Query port MTU, after setting new "
		    "MTU value, failed\n");
		ifp->if_mtu = sw_mtu;
	}
	return (0);
}

int
mlx5e_open_locked(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	int err;

	/* check if already opened */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0)
		return (0);

#ifdef RSS
	if (rss_getnumbuckets() > priv->params.num_channels) {
		if_printf(ifp, "NOTE: There are more RSS buckets(%u) than "
		    "channels(%u) available\n", rss_getnumbuckets(),
		    priv->params.num_channels);
	}
#endif
	err = mlx5e_open_tises(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_open_tises failed, %d\n",
		    __func__, err);
		return (err);
	}
	err = mlx5_vport_alloc_q_counter(priv->mdev, &priv->counter_set_id);
	if (err) {
		if_printf(priv->ifp,
		    "%s: mlx5_vport_alloc_q_counter failed: %d\n",
		    __func__, err);
		goto err_close_tises;
	}
	err = mlx5e_open_channels(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_open_channels failed, %d\n",
		    __func__, err);
		goto err_dalloc_q_counter;
	}
	err = mlx5e_open_rqt(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_open_rqt failed, %d\n",
		    __func__, err);
		goto err_close_channels;
	}
	err = mlx5e_open_tirs(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_open_tir failed, %d\n",
		    __func__, err);
		goto err_close_rqls;
	}
	err = mlx5e_open_flow_table(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_open_flow_table failed, %d\n",
		    __func__, err);
		goto err_close_tirs;
	}
	err = mlx5e_add_all_vlan_rules(priv);
	if (err) {
		if_printf(ifp, "%s: mlx5e_add_all_vlan_rules failed, %d\n",
		    __func__, err);
		goto err_close_flow_table;
	}
	set_bit(MLX5E_STATE_OPENED, &priv->state);

	mlx5e_update_carrier(priv);
	mlx5e_set_rx_mode_core(priv);

	return (0);

err_close_flow_table:
	mlx5e_close_flow_table(priv);

err_close_tirs:
	mlx5e_close_tirs(priv);

err_close_rqls:
	mlx5e_close_rqt(priv);

err_close_channels:
	mlx5e_close_channels(priv);

err_dalloc_q_counter:
	mlx5_vport_dealloc_q_counter(priv->mdev, priv->counter_set_id);

err_close_tises:
	mlx5e_close_tises(priv);

	return (err);
}

static void
mlx5e_open(void *arg)
{
	struct mlx5e_priv *priv = arg;

	PRIV_LOCK(priv);
	if (mlx5_set_port_status(priv->mdev, MLX5_PORT_UP))
		if_printf(priv->ifp,
		    "%s: Setting port status to up failed\n",
		    __func__);

	mlx5e_open_locked(priv->ifp);
	priv->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	PRIV_UNLOCK(priv);
}

int
mlx5e_close_locked(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;

	/* check if already closed */
	if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
		return (0);

	clear_bit(MLX5E_STATE_OPENED, &priv->state);

	mlx5e_set_rx_mode_core(priv);
	mlx5e_del_all_vlan_rules(priv);
	if_link_state_change(priv->ifp, LINK_STATE_DOWN);
	mlx5e_close_flow_table(priv);
	mlx5e_close_tirs(priv);
	mlx5e_close_rqt(priv);
	mlx5e_close_channels(priv);
	mlx5_vport_dealloc_q_counter(priv->mdev, priv->counter_set_id);
	mlx5e_close_tises(priv);

	return (0);
}

#if (__FreeBSD_version >= 1100000)
static uint64_t
mlx5e_get_counter(struct ifnet *ifp, ift_counter cnt)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	u64 retval;

	/* PRIV_LOCK(priv); XXX not allowed */
	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		retval = priv->stats.vport.rx_packets;
		break;
	case IFCOUNTER_IERRORS:
		retval = priv->stats.vport.rx_error_packets;
		break;
	case IFCOUNTER_IQDROPS:
		retval = priv->stats.vport.rx_out_of_buffer;
		break;
	case IFCOUNTER_OPACKETS:
		retval = priv->stats.vport.tx_packets;
		break;
	case IFCOUNTER_OERRORS:
		retval = priv->stats.vport.tx_error_packets;
		break;
	case IFCOUNTER_IBYTES:
		retval = priv->stats.vport.rx_bytes;
		break;
	case IFCOUNTER_OBYTES:
		retval = priv->stats.vport.tx_bytes;
		break;
	case IFCOUNTER_IMCASTS:
		retval = priv->stats.vport.rx_multicast_packets;
		break;
	case IFCOUNTER_OMCASTS:
		retval = priv->stats.vport.tx_multicast_packets;
		break;
	case IFCOUNTER_OQDROPS:
		retval = priv->stats.vport.tx_queue_dropped;
		break;
	default:
		retval = if_get_counter_default(ifp, cnt);
		break;
	}
	/* PRIV_UNLOCK(priv); XXX not allowed */
	return (retval);
}
#endif

static void
mlx5e_set_rx_mode(struct ifnet *ifp)
{
	struct mlx5e_priv *priv = ifp->if_softc;

	schedule_work(&priv->set_rx_mode_work);
}

static int
mlx5e_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct mlx5e_priv *priv;
	struct ifreq *ifr;
	struct ifi2creq i2c;
	int error = 0;
	int mask = 0;
	int size_read = 0;
	int module_num;
	int max_mtu;
	uint8_t read_addr;

	priv = ifp->if_softc;

	/* check if detaching */
	if (priv == NULL || priv->gone != 0)
		return (ENXIO);

	switch (command) {
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;

		PRIV_LOCK(priv);
		mlx5_query_port_max_mtu(priv->mdev, &max_mtu);

		if (ifr->ifr_mtu >= MLX5E_MTU_MIN &&
		    ifr->ifr_mtu <= MIN(MLX5E_MTU_MAX, max_mtu)) {
			int was_opened;

			was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);
			if (was_opened)
				mlx5e_close_locked(ifp);

			/* set new MTU */
			mlx5e_set_dev_port_mtu(ifp, ifr->ifr_mtu);

			if (was_opened)
				mlx5e_open_locked(ifp);
		} else {
			error = EINVAL;
			if_printf(ifp, "Invalid MTU value. Min val: %d, Max val: %d\n",
			    MLX5E_MTU_MIN, MIN(MLX5E_MTU_MAX, max_mtu));
		}
		PRIV_UNLOCK(priv);
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			mlx5e_set_rx_mode(ifp);
			break;
		}
		PRIV_LOCK(priv);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				if (test_bit(MLX5E_STATE_OPENED, &priv->state) == 0)
					mlx5e_open_locked(ifp);
				ifp->if_drv_flags |= IFF_DRV_RUNNING;
				mlx5_set_port_status(priv->mdev, MLX5_PORT_UP);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				mlx5_set_port_status(priv->mdev,
				    MLX5_PORT_DOWN);
				if (test_bit(MLX5E_STATE_OPENED, &priv->state) != 0)
					mlx5e_close_locked(ifp);
				mlx5e_update_carrier(priv);
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			}
		}
		PRIV_UNLOCK(priv);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mlx5e_set_rx_mode(ifp);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
	case SIOCGIFXMEDIA:
		ifr = (struct ifreq *)data;
		error = ifmedia_ioctl(ifp, ifr, &priv->media, command);
		break;
	case SIOCSIFCAP:
		ifr = (struct ifreq *)data;
		PRIV_LOCK(priv);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);

			if (IFCAP_TSO4 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO4;
				ifp->if_hwassist &= ~CSUM_IP_TSO;
				if_printf(ifp,
				    "tso4 disabled due to -txcsum.\n");
			}
		}
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
			ifp->if_hwassist ^= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);

			if (IFCAP_TSO6 & ifp->if_capenable &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				ifp->if_capenable &= ~IFCAP_TSO6;
				ifp->if_hwassist &= ~CSUM_IP6_TSO;
				if_printf(ifp,
				    "tso6 disabled due to -txcsum6.\n");
			}
		}
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
		if (mask & IFCAP_TSO4) {
			if (!(IFCAP_TSO4 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM & ifp->if_capenable)) {
				if_printf(ifp, "enable txcsum first.\n");
				error = EAGAIN;
				goto out;
			}
			ifp->if_capenable ^= IFCAP_TSO4;
			ifp->if_hwassist ^= CSUM_IP_TSO;
		}
		if (mask & IFCAP_TSO6) {
			if (!(IFCAP_TSO6 & ifp->if_capenable) &&
			    !(IFCAP_TXCSUM_IPV6 & ifp->if_capenable)) {
				if_printf(ifp, "enable txcsum6 first.\n");
				error = EAGAIN;
				goto out;
			}
			ifp->if_capenable ^= IFCAP_TSO6;
			ifp->if_hwassist ^= CSUM_IP6_TSO;
		}
		if (mask & IFCAP_VLAN_HWFILTER) {
			if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
				mlx5e_disable_vlan_filter(priv);
			else
				mlx5e_enable_vlan_filter(priv);

			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
		}
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (mask & IFCAP_WOL_MAGIC)
			ifp->if_capenable ^= IFCAP_WOL_MAGIC;

		VLAN_CAPABILITIES(ifp);
		/* turn off LRO means also turn of HW LRO - if it's on */
		if (mask & IFCAP_LRO) {
			int was_opened = test_bit(MLX5E_STATE_OPENED, &priv->state);
			bool need_restart = false;

			ifp->if_capenable ^= IFCAP_LRO;
			if (!(ifp->if_capenable & IFCAP_LRO)) {
				if (priv->params.hw_lro_en) {
					priv->params.hw_lro_en = false;
					need_restart = true;
					/* Not sure this is the correct way */
					priv->params_ethtool.hw_lro = priv->params.hw_lro_en;
				}
			}
			if (was_opened && need_restart) {
				mlx5e_close_locked(ifp);
				mlx5e_open_locked(ifp);
			}
		}
out:
		PRIV_UNLOCK(priv);
		break;

	case SIOCGI2C:
		ifr = (struct ifreq *)data;

		/*
		 * Copy from the user-space address ifr_data to the
		 * kernel-space address i2c
		 */
		error = copyin(ifr->ifr_data, &i2c, sizeof(i2c));
		if (error)
			break;

		if (i2c.len > sizeof(i2c.data)) {
			error = EINVAL;
			break;
		}

		PRIV_LOCK(priv);
		/* Get module_num which is required for the query_eeprom */
		error = mlx5_query_module_num(priv->mdev, &module_num);
		if (error) {
			if_printf(ifp, "Query module num failed, eeprom "
			    "reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}
		/* Check if module is present before doing an access */
		if (mlx5_query_module_status(priv->mdev, module_num) !=
		    MLX5_MODULE_STATUS_PLUGGED) {
			error = EINVAL;
			goto err_i2c;
		}
		/*
		 * Currently 0XA0 and 0xA2 are the only addresses permitted.
		 * The internal conversion is as follows:
		 */
		if (i2c.dev_addr == 0xA0)
			read_addr = MLX5E_I2C_ADDR_LOW;
		else if (i2c.dev_addr == 0xA2)
			read_addr = MLX5E_I2C_ADDR_HIGH;
		else {
			if_printf(ifp, "Query eeprom failed, "
			    "Invalid Address: %X\n", i2c.dev_addr);
			error = EINVAL;
			goto err_i2c;
		}
		error = mlx5_query_eeprom(priv->mdev,
		    read_addr, MLX5E_EEPROM_LOW_PAGE,
		    (uint32_t)i2c.offset, (uint32_t)i2c.len, module_num,
		    (uint32_t *)i2c.data, &size_read);
		if (error) {
			if_printf(ifp, "Query eeprom failed, eeprom "
			    "reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}

		if (i2c.len > MLX5_EEPROM_MAX_BYTES) {
			error = mlx5_query_eeprom(priv->mdev,
			    read_addr, MLX5E_EEPROM_LOW_PAGE,
			    (uint32_t)(i2c.offset + size_read),
			    (uint32_t)(i2c.len - size_read), module_num,
			    (uint32_t *)(i2c.data + size_read), &size_read);
		}
		if (error) {
			if_printf(ifp, "Query eeprom failed, eeprom "
			    "reading is not supported\n");
			error = EINVAL;
			goto err_i2c;
		}

		error = copyout(&i2c, ifr->ifr_data, sizeof(i2c));
err_i2c:
		PRIV_UNLOCK(priv);
		break;

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	return (error);
}

static int
mlx5e_check_required_hca_cap(struct mlx5_core_dev *mdev)
{
	/*
	 * TODO: uncoment once FW really sets all these bits if
	 * (!mdev->caps.eth.rss_ind_tbl_cap || !mdev->caps.eth.csum_cap ||
	 * !mdev->caps.eth.max_lso_cap || !mdev->caps.eth.vlan_cap ||
	 * !(mdev->caps.gen.flags & MLX5_DEV_CAP_FLAG_SCQE_BRK_MOD)) return
	 * -ENOTSUPP;
	 */

	/* TODO: add more must-to-have features */

	return (0);
}

static void
mlx5e_build_ifp_priv(struct mlx5_core_dev *mdev,
    struct mlx5e_priv *priv,
    int num_comp_vectors)
{
	/*
	 * TODO: Consider link speed for setting "log_sq_size",
	 * "log_rq_size" and "cq_moderation_xxx":
	 */
	priv->params.log_sq_size =
	    MLX5E_PARAMS_DEFAULT_LOG_SQ_SIZE;
	priv->params.log_rq_size =
	    MLX5E_PARAMS_DEFAULT_LOG_RQ_SIZE;
	priv->params.rx_cq_moderation_usec =
	    MLX5_CAP_GEN(mdev, cq_period_start_from_cqe) ?
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_USEC_FROM_CQE :
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_USEC;
	priv->params.rx_cq_moderation_mode =
	    MLX5_CAP_GEN(mdev, cq_period_start_from_cqe) ? 1 : 0;
	priv->params.rx_cq_moderation_pkts =
	    MLX5E_PARAMS_DEFAULT_RX_CQ_MODERATION_PKTS;
	priv->params.tx_cq_moderation_usec =
	    MLX5E_PARAMS_DEFAULT_TX_CQ_MODERATION_USEC;
	priv->params.tx_cq_moderation_pkts =
	    MLX5E_PARAMS_DEFAULT_TX_CQ_MODERATION_PKTS;
	priv->params.min_rx_wqes =
	    MLX5E_PARAMS_DEFAULT_MIN_RX_WQES;
	priv->params.rx_hash_log_tbl_sz =
	    (order_base_2(num_comp_vectors) >
	    MLX5E_PARAMS_DEFAULT_RX_HASH_LOG_TBL_SZ) ?
	    order_base_2(num_comp_vectors) :
	    MLX5E_PARAMS_DEFAULT_RX_HASH_LOG_TBL_SZ;
	priv->params.num_tc = 1;
	priv->params.default_vlan_prio = 0;
	priv->counter_set_id = -1;

	/*
	 * hw lro is currently defaulted to off. when it won't anymore we
	 * will consider the HW capability: "!!MLX5_CAP_ETH(mdev, lro_cap)"
	 */
	priv->params.hw_lro_en = false;
	priv->params.lro_wqe_sz = MLX5E_PARAMS_DEFAULT_LRO_WQE_SZ;

	priv->params.cqe_zipping_en = !!MLX5_CAP_GEN(mdev, cqe_compression);

	priv->mdev = mdev;
	priv->params.num_channels = num_comp_vectors;
	priv->order_base_2_num_channels = order_base_2(num_comp_vectors);
	priv->queue_mapping_channel_mask =
	    roundup_pow_of_two(num_comp_vectors) - 1;
	priv->num_tc = priv->params.num_tc;
	priv->default_vlan_prio = priv->params.default_vlan_prio;

	INIT_WORK(&priv->update_stats_work, mlx5e_update_stats_work);
	INIT_WORK(&priv->update_carrier_work, mlx5e_update_carrier_work);
	INIT_WORK(&priv->set_rx_mode_work, mlx5e_set_rx_mode_work);
}

static int
mlx5e_create_mkey(struct mlx5e_priv *priv, u32 pdn,
    struct mlx5_core_mr *mr)
{
	struct ifnet *ifp = priv->ifp;
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_create_mkey_mbox_in *in;
	int err;

	in = mlx5_vzalloc(sizeof(*in));
	if (in == NULL) {
		if_printf(ifp, "%s: failed to allocate inbox\n", __func__);
		return (-ENOMEM);
	}
	in->seg.flags = MLX5_PERM_LOCAL_WRITE |
	    MLX5_PERM_LOCAL_READ |
	    MLX5_ACCESS_MODE_PA;
	in->seg.flags_pd = cpu_to_be32(pdn | MLX5_MKEY_LEN64);
	in->seg.qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);

	err = mlx5_core_create_mkey(mdev, mr, in, sizeof(*in), NULL, NULL,
	    NULL);
	if (err)
		if_printf(ifp, "%s: mlx5_core_create_mkey failed, %d\n",
		    __func__, err);

	kvfree(in);

	return (err);
}

static const char *mlx5e_vport_stats_desc[] = {
	MLX5E_VPORT_STATS(MLX5E_STATS_DESC)
};

static const char *mlx5e_pport_stats_desc[] = {
	MLX5E_PPORT_STATS(MLX5E_STATS_DESC)
};

static void
mlx5e_priv_mtx_init(struct mlx5e_priv *priv)
{
	mtx_init(&priv->async_events_mtx, "mlx5async", MTX_NETWORK_LOCK, MTX_DEF);
	sx_init(&priv->state_lock, "mlx5state");
	callout_init_mtx(&priv->watchdog, &priv->async_events_mtx, 0);
}

static void
mlx5e_priv_mtx_destroy(struct mlx5e_priv *priv)
{
	mtx_destroy(&priv->async_events_mtx);
	sx_destroy(&priv->state_lock);
}

static int
sysctl_firmware(SYSCTL_HANDLER_ARGS)
{
	/*
	 * %d.%d%.d the string format.
	 * fw_rev_{maj,min,sub} return u16, 2^16 = 65536.
	 * We need at most 5 chars to store that.
	 * It also has: two "." and NULL at the end, which means we need 18
	 * (5*3 + 3) chars at most.
	 */
	char fw[18];
	struct mlx5e_priv *priv = arg1;
	int error;

	snprintf(fw, sizeof(fw), "%d.%d.%d", fw_rev_maj(priv->mdev), fw_rev_min(priv->mdev),
	    fw_rev_sub(priv->mdev));
	error = sysctl_handle_string(oidp, fw, sizeof(fw), req);
	return (error);
}

static void
mlx5e_add_hw_stats(struct mlx5e_priv *priv)
{
	SYSCTL_ADD_PROC(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_hw),
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD, priv, 0,
	    sysctl_firmware, "A", "HCA firmware version");

	SYSCTL_ADD_STRING(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_hw),
	    OID_AUTO, "board_id", CTLFLAG_RD, priv->mdev->board_id, 0,
	    "Board ID");
}

static void
mlx5e_setup_pauseframes(struct mlx5e_priv *priv)
{
#if (__FreeBSD_version < 1100000)
	char path[64];

#endif
	/* Only receiving pauseframes is enabled by default */
	priv->params.tx_pauseframe_control = 0;
	priv->params.rx_pauseframe_control = 1;

#if (__FreeBSD_version < 1100000)
	/* compute path for sysctl */
	snprintf(path, sizeof(path), "dev.mce.%d.tx_pauseframe_control",
	    device_get_unit(priv->mdev->pdev->dev.bsddev));

	/* try to fetch tunable, if any */
	TUNABLE_INT_FETCH(path, &priv->params.tx_pauseframe_control);

	/* compute path for sysctl */
	snprintf(path, sizeof(path), "dev.mce.%d.rx_pauseframe_control",
	    device_get_unit(priv->mdev->pdev->dev.bsddev));

	/* try to fetch tunable, if any */
	TUNABLE_INT_FETCH(path, &priv->params.rx_pauseframe_control);
#endif

	/* register pausframe SYSCTLs */
	SYSCTL_ADD_INT(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "tx_pauseframe_control", CTLFLAG_RDTUN,
	    &priv->params.tx_pauseframe_control, 0,
	    "Set to enable TX pause frames. Clear to disable.");

	SYSCTL_ADD_INT(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, "rx_pauseframe_control", CTLFLAG_RDTUN,
	    &priv->params.rx_pauseframe_control, 0,
	    "Set to enable RX pause frames. Clear to disable.");

	/* range check */
	priv->params.tx_pauseframe_control =
	    priv->params.tx_pauseframe_control ? 1 : 0;
	priv->params.rx_pauseframe_control =
	    priv->params.rx_pauseframe_control ? 1 : 0;

	/* update firmware */
	mlx5_set_port_pause(priv->mdev, 1,
	    priv->params.rx_pauseframe_control,
	    priv->params.tx_pauseframe_control);
}

static void *
mlx5e_create_ifp(struct mlx5_core_dev *mdev)
{
	static volatile int mlx5_en_unit;
	struct ifnet *ifp;
	struct mlx5e_priv *priv;
	u8 dev_addr[ETHER_ADDR_LEN] __aligned(4);
	struct sysctl_oid_list *child;
	int ncv = mdev->priv.eq_table.num_comp_vectors;
	char unit[16];
	int err;
	int i;
	u32 eth_proto_cap;

	if (mlx5e_check_required_hca_cap(mdev)) {
		mlx5_core_dbg(mdev, "mlx5e_check_required_hca_cap() failed\n");
		return (NULL);
	}
	priv = malloc(sizeof(*priv), M_MLX5EN, M_WAITOK | M_ZERO);
	if (priv == NULL) {
		mlx5_core_err(mdev, "malloc() failed\n");
		return (NULL);
	}
	mlx5e_priv_mtx_init(priv);

	ifp = priv->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		mlx5_core_err(mdev, "if_alloc() failed\n");
		goto err_free_priv;
	}
	ifp->if_softc = priv;
	if_initname(ifp, "mce", atomic_fetchadd_int(&mlx5_en_unit, 1));
	ifp->if_mtu = ETHERMTU;
	ifp->if_init = mlx5e_open;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = mlx5e_ioctl;
	ifp->if_transmit = mlx5e_xmit;
	ifp->if_qflush = if_qflush;
#if (__FreeBSD_version >= 1100000)
	ifp->if_get_counter = mlx5e_get_counter;
#endif
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	/*
         * Set driver features
         */
	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWFILTER;
	ifp->if_capabilities |= IFCAP_LINKSTATE | IFCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_LRO;
	ifp->if_capabilities |= IFCAP_TSO | IFCAP_VLAN_HWTSO;

	/* set TSO limits so that we don't have to drop TX packets */
	ifp->if_hw_tsomax = MLX5E_MAX_TX_PAYLOAD_SIZE - (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
	ifp->if_hw_tsomaxsegcount = MLX5E_MAX_TX_MBUF_FRAGS - 1 /* hdr */;
	ifp->if_hw_tsomaxsegsize = MLX5E_MAX_TX_MBUF_SIZE;

	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP | CSUM_IP);
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= (CSUM_UDP_IPV6 | CSUM_TCP_IPV6);

	/* ifnet sysctl tree */
	sysctl_ctx_init(&priv->sysctl_ctx);
	priv->sysctl_ifnet = SYSCTL_ADD_NODE(&priv->sysctl_ctx, SYSCTL_STATIC_CHILDREN(_dev),
	    OID_AUTO, ifp->if_dname, CTLFLAG_RD, 0, "MLX5 ethernet - interface name");
	if (priv->sysctl_ifnet == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}
	snprintf(unit, sizeof(unit), "%d", ifp->if_dunit);
	priv->sysctl_ifnet = SYSCTL_ADD_NODE(&priv->sysctl_ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    OID_AUTO, unit, CTLFLAG_RD, 0, "MLX5 ethernet - interface unit");
	if (priv->sysctl_ifnet == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}

	/* HW sysctl tree */
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(mdev->pdev->dev.bsddev));
	priv->sysctl_hw = SYSCTL_ADD_NODE(&priv->sysctl_ctx, child,
	    OID_AUTO, "hw", CTLFLAG_RD, 0, "MLX5 ethernet dev hw");
	if (priv->sysctl_hw == NULL) {
		mlx5_core_err(mdev, "SYSCTL_ADD_NODE() failed\n");
		goto err_free_sysctl;
	}
	mlx5e_build_ifp_priv(mdev, priv, ncv);
	err = mlx5_alloc_map_uar(mdev, &priv->cq_uar);
	if (err) {
		if_printf(ifp, "%s: mlx5_alloc_map_uar failed, %d\n",
		    __func__, err);
		goto err_free_sysctl;
	}
	err = mlx5_core_alloc_pd(mdev, &priv->pdn);
	if (err) {
		if_printf(ifp, "%s: mlx5_core_alloc_pd failed, %d\n",
		    __func__, err);
		goto err_unmap_free_uar;
	}
	err = mlx5_alloc_transport_domain(mdev, &priv->tdn);
	if (err) {
		if_printf(ifp, "%s: mlx5_alloc_transport_domain failed, %d\n",
		    __func__, err);
		goto err_dealloc_pd;
	}
	err = mlx5e_create_mkey(priv, priv->pdn, &priv->mr);
	if (err) {
		if_printf(ifp, "%s: mlx5e_create_mkey failed, %d\n",
		    __func__, err);
		goto err_dealloc_transport_domain;
	}
	mlx5_query_nic_vport_mac_address(priv->mdev, 0, dev_addr);

	/* check if we should generate a random MAC address */
	if (MLX5_CAP_GEN(priv->mdev, vport_group_manager) == 0 &&
	    is_zero_ether_addr(dev_addr)) {
		random_ether_addr(dev_addr);
		if_printf(ifp, "Assigned random MAC address\n");
	}

	/* set default MTU */
	mlx5e_set_dev_port_mtu(ifp, ifp->if_mtu);

	/* Set desc */
	device_set_desc(mdev->pdev->dev.bsddev, mlx5e_version);

	/* Set default media status */
	priv->media_status_last = IFM_AVALID;
	priv->media_active_last = IFM_ETHER | IFM_AUTO |
	    IFM_ETH_RXPAUSE | IFM_FDX;

	/* setup default pauseframes configuration */
	mlx5e_setup_pauseframes(priv);

	err = mlx5_query_port_proto_cap(mdev, &eth_proto_cap, MLX5_PTYS_EN);
	if (err) {
		eth_proto_cap = 0;
		if_printf(ifp, "%s: Query port media capability failed, %d\n",
		    __func__, err);
	}

	/* Setup supported medias */
	ifmedia_init(&priv->media, IFM_IMASK | IFM_ETH_FMASK,
	    mlx5e_media_change, mlx5e_media_status);

	for (i = 0; i < MLX5E_LINK_MODES_NUMBER; ++i) {
		if (mlx5e_mode_table[i].baudrate == 0)
			continue;
		if (MLX5E_PROT_MASK(i) & eth_proto_cap) {
			ifmedia_add(&priv->media,
			    mlx5e_mode_table[i].subtype |
			    IFM_ETHER, 0, NULL);
			ifmedia_add(&priv->media,
			    mlx5e_mode_table[i].subtype |
			    IFM_ETHER | IFM_FDX |
			    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE, 0, NULL);
		}
	}

	ifmedia_add(&priv->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_AUTO | IFM_FDX |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE, 0, NULL);

	/* Set autoselect by default */
	ifmedia_set(&priv->media, IFM_ETHER | IFM_AUTO | IFM_FDX |
	    IFM_ETH_RXPAUSE | IFM_ETH_TXPAUSE);
	ether_ifattach(ifp, dev_addr);

	/* Register for VLAN events */
	priv->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    mlx5e_vlan_rx_add_vid, priv, EVENTHANDLER_PRI_FIRST);
	priv->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    mlx5e_vlan_rx_kill_vid, priv, EVENTHANDLER_PRI_FIRST);

	/* Link is down by default */
	if_link_state_change(ifp, LINK_STATE_DOWN);

	mlx5e_enable_async_events(priv);

	mlx5e_add_hw_stats(priv);

	mlx5e_create_stats(&priv->stats.vport.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    "vstats", mlx5e_vport_stats_desc, MLX5E_VPORT_STATS_NUM,
	    priv->stats.vport.arg);

	mlx5e_create_stats(&priv->stats.pport.ctx, SYSCTL_CHILDREN(priv->sysctl_ifnet),
	    "pstats", mlx5e_pport_stats_desc, MLX5E_PPORT_STATS_NUM,
	    priv->stats.pport.arg);

	mlx5e_create_ethtool(priv);

	mtx_lock(&priv->async_events_mtx);
	mlx5e_update_stats(priv);
	mtx_unlock(&priv->async_events_mtx);

	return (priv);

err_dealloc_transport_domain:
	mlx5_dealloc_transport_domain(mdev, priv->tdn);

err_dealloc_pd:
	mlx5_core_dealloc_pd(mdev, priv->pdn);

err_unmap_free_uar:
	mlx5_unmap_free_uar(mdev, &priv->cq_uar);

err_free_sysctl:
	sysctl_ctx_free(&priv->sysctl_ctx);

	if_free(ifp);

err_free_priv:
	mlx5e_priv_mtx_destroy(priv);
	free(priv, M_MLX5EN);
	return (NULL);
}

static void
mlx5e_destroy_ifp(struct mlx5_core_dev *mdev, void *vpriv)
{
	struct mlx5e_priv *priv = vpriv;
	struct ifnet *ifp = priv->ifp;

	/* don't allow more IOCTLs */
	priv->gone = 1;

	/* XXX wait a bit to allow IOCTL handlers to complete */
	pause("W", hz);

	/* stop watchdog timer */
	callout_drain(&priv->watchdog);

	if (priv->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, priv->vlan_attach);
	if (priv->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, priv->vlan_detach);

	/* make sure device gets closed */
	PRIV_LOCK(priv);
	mlx5e_close_locked(ifp);
	PRIV_UNLOCK(priv);

	/* unregister device */
	ifmedia_removeall(&priv->media);
	ether_ifdetach(ifp);
	if_free(ifp);

	/* destroy all remaining sysctl nodes */
	if (priv->sysctl_debug)
		sysctl_ctx_free(&priv->stats.port_stats_debug.ctx);
	sysctl_ctx_free(&priv->stats.vport.ctx);
	sysctl_ctx_free(&priv->stats.pport.ctx);
	sysctl_ctx_free(&priv->sysctl_ctx);

	mlx5_core_destroy_mkey(priv->mdev, &priv->mr);
	mlx5_dealloc_transport_domain(priv->mdev, priv->tdn);
	mlx5_core_dealloc_pd(priv->mdev, priv->pdn);
	mlx5_unmap_free_uar(priv->mdev, &priv->cq_uar);
	mlx5e_disable_async_events(priv);
	flush_scheduled_work();
	mlx5e_priv_mtx_destroy(priv);
	free(priv, M_MLX5EN);
}

static void *
mlx5e_get_ifp(void *vpriv)
{
	struct mlx5e_priv *priv = vpriv;

	return (priv->ifp);
}

static struct mlx5_interface mlx5e_interface = {
	.add = mlx5e_create_ifp,
	.remove = mlx5e_destroy_ifp,
	.event = mlx5e_async_event,
	.protocol = MLX5_INTERFACE_PROTOCOL_ETH,
	.get_dev = mlx5e_get_ifp,
};

void
mlx5e_init(void)
{
	mlx5_register_interface(&mlx5e_interface);
}

void
mlx5e_cleanup(void)
{
	mlx5_unregister_interface(&mlx5e_interface);
}

module_init_order(mlx5e_init, SI_ORDER_THIRD);
module_exit_order(mlx5e_cleanup, SI_ORDER_THIRD);

#if (__FreeBSD_version >= 1100000)
MODULE_DEPEND(mlx5en, linuxkpi, 1, 1, 1);
#endif
MODULE_DEPEND(mlx5en, mlx5, 1, 1, 1);
MODULE_VERSION(mlx5en, 1);
