/*-
 * Copyright (c) 2009-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/sfxge/common/siena_nic.c 300607 2016-05-24 12:16:57Z arybchik $");

#include "efx.h"
#include "efx_impl.h"
#include "mcdi_mon.h"

#if EFSYS_OPT_SIENA

static	__checkReturn		efx_rc_t
siena_nic_get_partn_mask(
	__in			efx_nic_t *enp,
	__out			unsigned int *maskp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_NVRAM_TYPES_IN_LEN,
			    MC_CMD_NVRAM_TYPES_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_NVRAM_TYPES;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_NVRAM_TYPES_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_NVRAM_TYPES_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_NVRAM_TYPES_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*maskp = MCDI_OUT_DWORD(req, NVRAM_TYPES_OUT_TYPES);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
siena_board_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint8_t mac_addr[6];
	efx_dword_t capabilities;
	uint32_t board_type;
	uint32_t nevq, nrxq, ntxq;
	efx_rc_t rc;

	/* External port identifier using one-based port numbering */
	encp->enc_external_port = (uint8_t)enp->en_mcdi.em_emip.emi_port;

	/* Board configuration */
	if ((rc = efx_mcdi_get_board_cfg(enp, &board_type,
		    &capabilities, mac_addr)) != 0)
		goto fail1;

	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, mac_addr);

	encp->enc_board_type = board_type;

	/* Additional capabilities */
	encp->enc_clk_mult = 1;
	if (EFX_DWORD_FIELD(capabilities, MC_CMD_CAPABILITIES_TURBO)) {
		enp->en_features |= EFX_FEATURE_TURBO;

		if (EFX_DWORD_FIELD(capabilities,
			MC_CMD_CAPABILITIES_TURBO_ACTIVE)) {
			encp->enc_clk_mult = 2;
		}
	}

	encp->enc_evq_timer_quantum_ns =
		EFX_EVQ_SIENA_TIMER_QUANTUM_NS / encp->enc_clk_mult;
	encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		FRF_CZ_TC_TIMER_VAL_WIDTH) / 1000;

	/* When hash header insertion is enabled, Siena inserts 16 bytes */
	encp->enc_rx_prefix_size = 16;

	/* Alignment for receive packet DMA buffers */
	encp->enc_rx_buf_align_start = 1;
	encp->enc_rx_buf_align_end = 1;

	/* Alignment for WPTR updates */
	encp->enc_rx_push_align = 1;

	/* Resource limits */
	rc = efx_mcdi_get_resource_limits(enp, &nevq, &nrxq, &ntxq);
	if (rc != 0) {
		if (rc != ENOTSUP)
			goto fail2;

		nevq = 1024;
		nrxq = EFX_RXQ_LIMIT_TARGET;
		ntxq = EFX_TXQ_LIMIT_TARGET;
	}
	encp->enc_evq_limit = nevq;
	encp->enc_rxq_limit = MIN(EFX_RXQ_LIMIT_TARGET, nrxq);
	encp->enc_txq_limit = MIN(EFX_TXQ_LIMIT_TARGET, ntxq);

	encp->enc_buftbl_limit = SIENA_SRAM_ROWS -
	    (encp->enc_txq_limit * EFX_TXQ_DC_NDESCS(EFX_TXQ_DC_SIZE)) -
	    (encp->enc_rxq_limit * EFX_RXQ_DC_NDESCS(EFX_RXQ_DC_SIZE));

	encp->enc_hw_tx_insert_vlan_enabled = B_FALSE;
	encp->enc_fw_assisted_tso_enabled = B_FALSE;
	encp->enc_fw_assisted_tso_v2_enabled = B_FALSE;
	encp->enc_allow_set_mac_with_installed_filters = B_TRUE;

	/* Siena supports two 10G ports, and 8 lanes of PCIe Gen2 */
	encp->enc_required_pcie_bandwidth_mbps = 2 * 10000;
	encp->enc_max_pcie_link_gen = EFX_PCIE_LINK_SPEED_GEN2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
siena_phy_cfg(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	/* Fill out fields in enp->en_port and enp->en_nic_cfg from MCDI */
	if ((rc = efx_mcdi_get_phy_cfg(enp)) != 0)
		goto fail1;

#if EFSYS_OPT_PHY_STATS
	/* Convert the MCDI statistic mask into the EFX_PHY_STAT mask */
	siena_phy_decode_stats(enp, encp->enc_mcdi_phy_stat_mask,
			    NULL, &encp->enc_phy_stat_mask, NULL);
#endif	/* EFSYS_OPT_PHY_STATS */

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
siena_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	siena_link_state_t sls;
	unsigned int mask;
	efx_oword_t oword;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Test BIU */
	if ((rc = efx_nic_biu_test(enp)) != 0)
		goto fail1;

	/* Clear the region register */
	EFX_POPULATE_OWORD_4(oword,
	    FRF_AZ_ADR_REGION0, 0,
	    FRF_AZ_ADR_REGION1, (1 << 16),
	    FRF_AZ_ADR_REGION2, (2 << 16),
	    FRF_AZ_ADR_REGION3, (3 << 16));
	EFX_BAR_WRITEO(enp, FR_AZ_ADR_REGION_REG, &oword);

	/* Read clear any assertion state */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail2;

	/* Exit the assertion handler */
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail3;

	/* Wrestle control from the BMC */
	if ((rc = efx_mcdi_drv_attach(enp, B_TRUE)) != 0)
		goto fail4;

	if ((rc = siena_board_cfg(enp)) != 0)
		goto fail5;

	if ((rc = siena_phy_cfg(enp)) != 0)
		goto fail6;

	/* Obtain the default PHY advertised capabilities */
	if ((rc = siena_nic_reset(enp)) != 0)
		goto fail7;
	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail8;
	epp->ep_default_adv_cap_mask = sls.sls_adv_cap_mask;
	epp->ep_adv_cap_mask = sls.sls_adv_cap_mask;

#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM
	if ((rc = siena_nic_get_partn_mask(enp, &mask)) != 0)
		goto fail9;
	enp->en_u.siena.enu_partn_mask = mask;
#endif

#if EFSYS_OPT_MAC_STATS
	/* Wipe the MAC statistics */
	if ((rc = efx_mcdi_mac_stats_clear(enp)) != 0)
		goto fail10;
#endif

#if EFSYS_OPT_LOOPBACK
	if ((rc = efx_mcdi_get_loopback_modes(enp)) != 0)
		goto fail11;
#endif

#if EFSYS_OPT_MON_STATS
	if ((rc = mcdi_mon_cfg_build(enp)) != 0)
		goto fail12;
#endif

	encp->enc_features = enp->en_features;

	return (0);

#if EFSYS_OPT_MON_STATS
fail12:
	EFSYS_PROBE(fail12);
#endif
#if EFSYS_OPT_LOOPBACK
fail11:
	EFSYS_PROBE(fail11);
#endif
#if EFSYS_OPT_MAC_STATS
fail10:
	EFSYS_PROBE(fail10);
#endif
#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM
fail9:
	EFSYS_PROBE(fail9);
#endif
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
siena_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* siena_nic_reset() is called to recover from BADASSERT failures. */
	if ((rc = efx_mcdi_read_assertion(enp)) != 0)
		goto fail1;
	if ((rc = efx_mcdi_exit_assertion_handler(enp)) != 0)
		goto fail2;

	/*
	 * Bug24908: ENTITY_RESET_IN_LEN is non zero but zero may be supplied
	 * for backwards compatibility with PORT_RESET_IN_LEN.
	 */
	EFX_STATIC_ASSERT(MC_CMD_ENTITY_RESET_OUT_LEN == 0);

	req.emr_cmd = MC_CMD_ENTITY_RESET;
	req.emr_in_buf = NULL;
	req.emr_in_length = 0;
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (0);
}

static			void
siena_nic_rx_cfg(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;

	/*
	 * RX_INGR_EN is always enabled on Siena, because we rely on
	 * the RX parser to be resiliant to missing SOP/EOP.
	 */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_INGR_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Disable parsing of additional 802.1Q in Q packets */
	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_CZ_RX_FILTER_ALL_VLAN_ETHERTYPES, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
}

static			void
siena_nic_usrev_dis(
	__in		efx_nic_t *enp)
{
	efx_oword_t	oword;

	EFX_POPULATE_OWORD_1(oword, FRF_CZ_USREV_DIS, 1);
	EFX_BAR_WRITEO(enp, FR_CZ_USR_EV_CFG, &oword);
}

	__checkReturn	efx_rc_t
siena_nic_init(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_SIENA);

	/* Enable reporting of some events (e.g. link change) */
	if ((rc = efx_mcdi_log_ctrl(enp)) != 0)
		goto fail1;

	siena_sram_init(enp);

	/* Configure Siena's RX block */
	siena_nic_rx_cfg(enp);

	/* Disable USR_EVents for now */
	siena_nic_usrev_dis(enp);

	/* bug17057: Ensure set_link is called */
	if ((rc = siena_phy_reconfigure(enp)) != 0)
		goto fail2;

	enp->en_nic_cfg.enc_mcdi_max_payload_length = MCDI_CTL_SDU_LEN_MAX_V1;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
siena_nic_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

			void
siena_nic_unprobe(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_MON_STATS
	mcdi_mon_cfg_free(enp);
#endif /* EFSYS_OPT_MON_STATS */
	(void) efx_mcdi_drv_attach(enp, B_FALSE);
}

#if EFSYS_OPT_DIAG

static efx_register_set_t __siena_registers[] = {
	{ FR_AZ_ADR_REGION_REG_OFST, 0, 1 },
	{ FR_CZ_USR_EV_CFG_OFST, 0, 1 },
	{ FR_AZ_RX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_RESERVED_REG_OFST, 0, 1 },
	{ FR_AZ_SRM_TX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_PF_WM_REG_OFST, 0, 1 },
	{ FR_AZ_DP_CTRL_REG_OFST, 0, 1 },
	{ FR_BZ_RX_RSS_TKEY_REG_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG1_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG2_OFST, 0, 1},
	{ FR_CZ_RX_RSS_IPV6_REG3_OFST, 0, 1}
};

static const uint32_t __siena_register_masks[] = {
	0x0003FFFF, 0x0003FFFF, 0x0003FFFF, 0x0003FFFF,
	0x000103FF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFE, 0xFFFFFFFF, 0x0003FFFF, 0x00000000,
	0x7FFF0037, 0xFFFF8000, 0xFFFFFFFF, 0x03FFFFFF,
	0xFFFEFE80, 0x1FFFFFFF, 0x020000FE, 0x007FFFFF,
	0x001FFFFF, 0x00000000, 0x00000000, 0x00000000,
	0x00000003, 0x00000000, 0x00000000, 0x00000000,
	0x000003FF, 0x00000000, 0x00000000, 0x00000000,
	0x00000FFF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
	0xFFFFFFFF, 0xFFFFFFFF, 0x00000007, 0x00000000
};

static efx_register_set_t __siena_tables[] = {
	{ FR_AZ_RX_FILTER_TBL0_OFST, FR_AZ_RX_FILTER_TBL0_STEP,
	    FR_AZ_RX_FILTER_TBL0_ROWS },
	{ FR_CZ_RX_MAC_FILTER_TBL0_OFST, FR_CZ_RX_MAC_FILTER_TBL0_STEP,
	    FR_CZ_RX_MAC_FILTER_TBL0_ROWS },
	{ FR_AZ_RX_DESC_PTR_TBL_OFST,
	    FR_AZ_RX_DESC_PTR_TBL_STEP, FR_CZ_RX_DESC_PTR_TBL_ROWS },
	{ FR_AZ_TX_DESC_PTR_TBL_OFST,
	    FR_AZ_TX_DESC_PTR_TBL_STEP, FR_CZ_TX_DESC_PTR_TBL_ROWS },
	{ FR_AZ_TIMER_TBL_OFST, FR_AZ_TIMER_TBL_STEP, FR_CZ_TIMER_TBL_ROWS },
	{ FR_CZ_TX_FILTER_TBL0_OFST,
	    FR_CZ_TX_FILTER_TBL0_STEP, FR_CZ_TX_FILTER_TBL0_ROWS },
	{ FR_CZ_TX_MAC_FILTER_TBL0_OFST,
	    FR_CZ_TX_MAC_FILTER_TBL0_STEP, FR_CZ_TX_MAC_FILTER_TBL0_ROWS }
};

static const uint32_t __siena_table_masks[] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000003FF,
	0xFFFF0FFF, 0xFFFFFFFF, 0x00000E7F, 0x00000000,
	0xFFFFFFFE, 0x0FFFFFFF, 0x01800000, 0x00000000,
	0xFFFFFFFE, 0x0FFFFFFF, 0x0C000000, 0x00000000,
	0x3FFFFFFF, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000013FF,
	0xFFFF07FF, 0xFFFFFFFF, 0x0000007F, 0x00000000,
};

	__checkReturn	efx_rc_t
siena_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_register_set_t *rsp;
	const uint32_t *dwordp;
	unsigned int nitems;
	unsigned int count;
	efx_rc_t rc;

	/* Fill out the register mask entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__siena_register_masks)
		    == EFX_ARRAY_SIZE(__siena_registers) * 4);

	nitems = EFX_ARRAY_SIZE(__siena_registers);
	dwordp = __siena_register_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __siena_registers + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	/* Fill out the register table entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__siena_table_masks)
		    == EFX_ARRAY_SIZE(__siena_tables) * 4);

	nitems = EFX_ARRAY_SIZE(__siena_tables);
	dwordp = __siena_table_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __siena_tables + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	if ((rc = efx_nic_test_registers(enp, __siena_registers,
	    EFX_ARRAY_SIZE(__siena_registers))) != 0)
		goto fail1;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BYTE_ALTERNATE,
	    EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail2;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BYTE_CHANGING,
	    EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail3;

	if ((rc = efx_nic_test_tables(enp, __siena_tables,
	    EFX_PATTERN_BIT_SWEEP, EFX_ARRAY_SIZE(__siena_tables))) != 0)
		goto fail4;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

#endif	/* EFSYS_OPT_SIENA */
