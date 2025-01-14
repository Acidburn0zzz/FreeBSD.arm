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
 * $FreeBSD: head/sys/dev/mlx5/mlx5_en/mlx5_en_rx.c 301538 2016-06-07 04:51:50Z sephe $
 */

#include "en.h"
#include <machine/in_cksum.h>

static inline int
mlx5e_alloc_rx_wqe(struct mlx5e_rq *rq,
    struct mlx5e_rx_wqe *wqe, u16 ix)
{
	bus_dma_segment_t segs[1];
	struct mbuf *mb;
	int nsegs;
	int err;

	if (rq->mbuf[ix].mbuf != NULL)
		return (0);

	mb = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, rq->wqe_sz);
	if (unlikely(!mb))
		return (-ENOMEM);

	/* set initial mbuf length */
	mb->m_pkthdr.len = mb->m_len = rq->wqe_sz;

	/* get IP header aligned */
	m_adj(mb, MLX5E_NET_IP_ALIGN);

	err = -bus_dmamap_load_mbuf_sg(rq->dma_tag, rq->mbuf[ix].dma_map,
	    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	if (err != 0)
		goto err_free_mbuf;
	if (unlikely(nsegs != 1)) {
		bus_dmamap_unload(rq->dma_tag, rq->mbuf[ix].dma_map);
		err = -ENOMEM;
		goto err_free_mbuf;
	}
	wqe->data.addr = cpu_to_be64(segs[0].ds_addr);

	rq->mbuf[ix].mbuf = mb;
	rq->mbuf[ix].data = mb->m_data;

	bus_dmamap_sync(rq->dma_tag, rq->mbuf[ix].dma_map,
	    BUS_DMASYNC_PREREAD);
	return (0);

err_free_mbuf:
	m_freem(mb);
	return (err);
}

static void
mlx5e_post_rx_wqes(struct mlx5e_rq *rq)
{
	if (unlikely(rq->enabled == 0))
		return;

	while (!mlx5_wq_ll_is_full(&rq->wq)) {
		struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(&rq->wq, rq->wq.head);

		if (unlikely(mlx5e_alloc_rx_wqe(rq, wqe, rq->wq.head)))
			break;

		mlx5_wq_ll_push(&rq->wq, be16_to_cpu(wqe->next.next_wqe_index));
	}

	/* ensure wqes are visible to device before updating doorbell record */
	wmb();

	mlx5_wq_ll_update_db_record(&rq->wq);
}

static void
mlx5e_lro_update_hdr(struct mbuf *mb, struct mlx5_cqe64 *cqe)
{
	/* TODO: consider vlans, ip options, ... */
	struct ether_header *eh;
	uint16_t eh_type;
	uint16_t tot_len;
	struct ip6_hdr *ip6 = NULL;
	struct ip *ip4 = NULL;
	struct tcphdr *th;
	uint32_t *ts_ptr;
	uint8_t l4_hdr_type;
	int tcp_ack;

	eh = mtod(mb, struct ether_header *);
	eh_type = ntohs(eh->ether_type);

	l4_hdr_type = get_cqe_l4_hdr_type(cqe);
	tcp_ack = ((CQE_L4_HDR_TYPE_TCP_ACK_NO_DATA == l4_hdr_type) ||
	    (CQE_L4_HDR_TYPE_TCP_ACK_AND_DATA == l4_hdr_type));

	/* TODO: consider vlan */
	tot_len = be32_to_cpu(cqe->byte_cnt) - ETHER_HDR_LEN;

	switch (eh_type) {
	case ETHERTYPE_IP:
		ip4 = (struct ip *)(eh + 1);
		th = (struct tcphdr *)(ip4 + 1);
		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(eh + 1);
		th = (struct tcphdr *)(ip6 + 1);
		break;
	default:
		return;
	}

	ts_ptr = (uint32_t *)(th + 1);

	if (get_cqe_lro_tcppsh(cqe))
		th->th_flags |= TH_PUSH;

	if (tcp_ack) {
		th->th_flags |= TH_ACK;
		th->th_ack = cqe->lro_ack_seq_num;
		th->th_win = cqe->lro_tcp_win;

		/*
		 * FreeBSD handles only 32bit aligned timestamp right after
		 * the TCP hdr
		 * +--------+--------+--------+--------+
		 * |   NOP  |  NOP   |  TSopt |   10   |
		 * +--------+--------+--------+--------+
		 * |          TSval   timestamp        |
		 * +--------+--------+--------+--------+
		 * |          TSecr   timestamp        |
		 * +--------+--------+--------+--------+
		 */
		if (get_cqe_lro_timestamp_valid(cqe) &&
		    (__predict_true(*ts_ptr) == ntohl(TCPOPT_NOP << 24 |
		    TCPOPT_NOP << 16 | TCPOPT_TIMESTAMP << 8 |
		    TCPOLEN_TIMESTAMP))) {
			/*
			 * cqe->timestamp is 64bit long.
			 * [0-31] - timestamp.
			 * [32-64] - timestamp echo replay.
			 */
			ts_ptr[1] = *(uint32_t *)&cqe->timestamp;
			ts_ptr[2] = *((uint32_t *)&cqe->timestamp + 1);
		}
	}
	if (ip4) {
		ip4->ip_ttl = cqe->lro_min_ttl;
		ip4->ip_len = cpu_to_be16(tot_len);
		ip4->ip_sum = 0;
		ip4->ip_sum = in_cksum(mb, ip4->ip_hl << 2);
	} else {
		ip6->ip6_hlim = cqe->lro_min_ttl;
		ip6->ip6_plen = cpu_to_be16(tot_len -
		    sizeof(struct ip6_hdr));
	}
	/* TODO: handle tcp checksum */
}

static inline void
mlx5e_build_rx_mbuf(struct mlx5_cqe64 *cqe,
    struct mlx5e_rq *rq, struct mbuf *mb,
    u32 cqe_bcnt)
{
	struct ifnet *ifp = rq->ifp;
	int lro_num_seg;	/* HW LRO session aggregated packets counter */

	lro_num_seg = be32_to_cpu(cqe->srqn) >> 24;
	if (lro_num_seg > 1) {
		mlx5e_lro_update_hdr(mb, cqe);
		rq->stats.lro_packets++;
		rq->stats.lro_bytes += cqe_bcnt;
	}

	mb->m_pkthdr.len = mb->m_len = cqe_bcnt;
	/* check if a Toeplitz hash was computed */
	if (cqe->rss_hash_type != 0) {
		mb->m_pkthdr.flowid = be32_to_cpu(cqe->rss_hash_result);
#ifdef RSS
		/* decode the RSS hash type */
		switch (cqe->rss_hash_type &
		    (CQE_RSS_DST_HTYPE_L4 | CQE_RSS_DST_HTYPE_IP)) {
		/* IPv4 */
		case (CQE_RSS_DST_HTYPE_TCP | CQE_RSS_DST_HTYPE_IPV4):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_TCP_IPV4);
			break;
		case (CQE_RSS_DST_HTYPE_UDP | CQE_RSS_DST_HTYPE_IPV4):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_UDP_IPV4);
			break;
		case CQE_RSS_DST_HTYPE_IPV4:
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_IPV4);
			break;
		/* IPv6 */
		case (CQE_RSS_DST_HTYPE_TCP | CQE_RSS_DST_HTYPE_IPV6):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_TCP_IPV6);
			break;
		case (CQE_RSS_DST_HTYPE_UDP | CQE_RSS_DST_HTYPE_IPV6):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_UDP_IPV6);
			break;
		case CQE_RSS_DST_HTYPE_IPV6:
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_IPV6);
			break;
		default:	/* Other */
			M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE_HASH);
			break;
		}
#else
		M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE_HASH);
#endif
	} else {
		mb->m_pkthdr.flowid = rq->ix;
		M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE);
	}
	mb->m_pkthdr.rcvif = ifp;

	if (likely(ifp->if_capenable & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) &&
	    ((cqe->hds_ip_ext & (CQE_L2_OK | CQE_L3_OK | CQE_L4_OK)) ==
	    (CQE_L2_OK | CQE_L3_OK | CQE_L4_OK))) {
		mb->m_pkthdr.csum_flags =
		    CSUM_IP_CHECKED | CSUM_IP_VALID |
		    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		mb->m_pkthdr.csum_data = htons(0xffff);
	} else {
		rq->stats.csum_none++;
	}

	if (cqe_has_vlan(cqe)) {
		mb->m_pkthdr.ether_vtag = be16_to_cpu(cqe->vlan_info);
		mb->m_flags |= M_VLANTAG;
	}
}

static inline void
mlx5e_read_cqe_slot(struct mlx5e_cq *cq, u32 cc, void *data)
{
	memcpy(data, mlx5_cqwq_get_wqe(&cq->wq, (cc & cq->wq.sz_m1)),
	    sizeof(struct mlx5_cqe64));
}

static inline void
mlx5e_write_cqe_slot(struct mlx5e_cq *cq, u32 cc, void *data)
{
	memcpy(mlx5_cqwq_get_wqe(&cq->wq, cc & cq->wq.sz_m1),
	    data, sizeof(struct mlx5_cqe64));
}

static inline void
mlx5e_decompress_cqe(struct mlx5e_cq *cq, struct mlx5_cqe64 *title,
    struct mlx5_mini_cqe8 *mini,
    u16 wqe_counter, int i)
{
	/*
	 * NOTE: The fields which are not set here are copied from the
	 * initial and common title. See memcpy() in
	 * mlx5e_write_cqe_slot().
	 */
	title->byte_cnt = mini->byte_cnt;
	title->wqe_counter = cpu_to_be16((wqe_counter + i) & cq->wq.sz_m1);
	title->check_sum = mini->checksum;
	title->op_own = (title->op_own & 0xf0) |
	    (((cq->wq.cc + i) >> cq->wq.log_sz) & 1);
}

#define MLX5E_MINI_ARRAY_SZ 8
/* Make sure structs are not packet differently */
CTASSERT(sizeof(struct mlx5_cqe64) ==
    sizeof(struct mlx5_mini_cqe8) * MLX5E_MINI_ARRAY_SZ);
static void
mlx5e_decompress_cqes(struct mlx5e_cq *cq)
{
	struct mlx5_mini_cqe8 mini_array[MLX5E_MINI_ARRAY_SZ];
	struct mlx5_cqe64 title;
	u32 cqe_count;
	u32 i = 0;
	u16 title_wqe_counter;

	mlx5e_read_cqe_slot(cq, cq->wq.cc, &title);
	title_wqe_counter = be16_to_cpu(title.wqe_counter);
	cqe_count = be32_to_cpu(title.byte_cnt);

	/* Make sure we won't overflow */
	KASSERT(cqe_count <= cq->wq.sz_m1,
	    ("%s: cqe_count %u > cq->wq.sz_m1 %u", __func__,
	    cqe_count, cq->wq.sz_m1));

	mlx5e_read_cqe_slot(cq, cq->wq.cc + 1, mini_array);
	while (true) {
		mlx5e_decompress_cqe(cq, &title,
		    &mini_array[i % MLX5E_MINI_ARRAY_SZ],
		    title_wqe_counter, i);
		mlx5e_write_cqe_slot(cq, cq->wq.cc + i, &title);
		i++;

		if (i == cqe_count)
			break;
		if (i % MLX5E_MINI_ARRAY_SZ == 0)
			mlx5e_read_cqe_slot(cq, cq->wq.cc + i, mini_array);
	}
}

static int
mlx5e_poll_rx_cq(struct mlx5e_rq *rq, int budget)
{
	int i;

	for (i = 0; i < budget; i++) {
		struct mlx5e_rx_wqe *wqe;
		struct mlx5_cqe64 *cqe;
		struct mbuf *mb;
		__be16 wqe_counter_be;
		u16 wqe_counter;
		u32 byte_cnt;

		cqe = mlx5e_get_cqe(&rq->cq);
		if (!cqe)
			break;

		if (mlx5_get_cqe_format(cqe) == MLX5_COMPRESSED)
			mlx5e_decompress_cqes(&rq->cq);

		mlx5_cqwq_pop(&rq->cq.wq);

		wqe_counter_be = cqe->wqe_counter;
		wqe_counter = be16_to_cpu(wqe_counter_be);
		wqe = mlx5_wq_ll_get_wqe(&rq->wq, wqe_counter);
		byte_cnt = be32_to_cpu(cqe->byte_cnt);

		bus_dmamap_sync(rq->dma_tag,
		    rq->mbuf[wqe_counter].dma_map,
		    BUS_DMASYNC_POSTREAD);

		if (unlikely((cqe->op_own >> 4) != MLX5_CQE_RESP_SEND)) {
			rq->stats.wqe_err++;
			goto wq_ll_pop;
		}

		if (MHLEN >= byte_cnt &&
		    (mb = m_gethdr(M_NOWAIT, MT_DATA)) != NULL) {
			bcopy(rq->mbuf[wqe_counter].data, mtod(mb, caddr_t),
			    byte_cnt);
		} else {
			mb = rq->mbuf[wqe_counter].mbuf;
			rq->mbuf[wqe_counter].mbuf = NULL;	/* safety clear */

			bus_dmamap_unload(rq->dma_tag,
			    rq->mbuf[wqe_counter].dma_map);
		}

		mlx5e_build_rx_mbuf(cqe, rq, mb, byte_cnt);
		rq->stats.packets++;
#ifdef HAVE_TURBO_LRO
		if (mb->m_pkthdr.csum_flags == 0 ||
		    (rq->ifp->if_capenable & IFCAP_LRO) == 0 ||
		    rq->lro.mbuf == NULL) {
			/* normal input */
			rq->ifp->if_input(rq->ifp, mb);
		} else {
			tcp_tlro_rx(&rq->lro, mb);
		}
#else
		if (mb->m_pkthdr.csum_flags == 0 ||
		    (rq->ifp->if_capenable & IFCAP_LRO) == 0 ||
		    rq->lro.lro_cnt == 0 ||
		    tcp_lro_rx(&rq->lro, mb, 0) != 0) {
			rq->ifp->if_input(rq->ifp, mb);
		}
#endif
wq_ll_pop:
		mlx5_wq_ll_pop(&rq->wq, wqe_counter_be,
		    &wqe->next.next_wqe_index);
	}

	mlx5_cqwq_update_db_record(&rq->cq.wq);

	/* ensure cq space is freed before enabling more cqes */
	wmb();
#ifndef HAVE_TURBO_LRO
	tcp_lro_flush_all(&rq->lro);
#endif
	return (i);
}

void
mlx5e_rx_cq_comp(struct mlx5_core_cq *mcq)
{
	struct mlx5e_rq *rq = container_of(mcq, struct mlx5e_rq, cq.mcq);
	int i = 0;

#ifdef HAVE_PER_CQ_EVENT_PACKET
	struct mbuf *mb = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, rq->wqe_sz);

	if (mb != NULL) {
		/* this code is used for debugging purpose only */
		mb->m_pkthdr.len = mb->m_len = 15;
		memset(mb->m_data, 255, 14);
		mb->m_data[14] = rq->ix;
		mb->m_pkthdr.rcvif = rq->ifp;
		rq->ifp->if_input(rq->ifp, mb);
	}
#endif

	mtx_lock(&rq->mtx);

	/*
	 * Polling the entire CQ without posting new WQEs results in
	 * lack of receive WQEs during heavy traffic scenarios.
	 */
	while (1) {
		if (mlx5e_poll_rx_cq(rq, MLX5E_RX_BUDGET_MAX) !=
		    MLX5E_RX_BUDGET_MAX)
			break;
		i += MLX5E_RX_BUDGET_MAX;
		if (i >= MLX5E_BUDGET_MAX)
			break;
		mlx5e_post_rx_wqes(rq);
	}
	mlx5e_post_rx_wqes(rq);
	mlx5e_cq_arm(&rq->cq);
#ifdef HAVE_TURBO_LRO
	tcp_tlro_flush(&rq->lro, 1);
#endif
	mtx_unlock(&rq->mtx);
}
