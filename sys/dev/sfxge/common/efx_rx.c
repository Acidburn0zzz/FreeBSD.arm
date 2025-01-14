/*-
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
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
__FBSDID("$FreeBSD: head/sys/dev/sfxge/common/efx_rx.c 300840 2016-05-27 11:44:40Z arybchik $");

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
siena_rx_init(
	__in		efx_nic_t *enp);

static			void
siena_rx_fini(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_RX_SCATTER
static	__checkReturn	efx_rc_t
siena_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size);
#endif /* EFSYS_OPT_RX_SCATTER */

#if EFSYS_OPT_RX_SCALE
static	__checkReturn	efx_rc_t
siena_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert);

static	__checkReturn	efx_rc_t
siena_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n);

static	__checkReturn	efx_rc_t
siena_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n);

static	__checkReturn	uint32_t
siena_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer);

#endif /* EFSYS_OPT_RX_SCALE */

static	__checkReturn	efx_rc_t
siena_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp);

static			void
siena_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added);

static			void
siena_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp);

static	__checkReturn	efx_rc_t
siena_rx_qflush(
	__in		efx_rxq_t *erp);

static			void
siena_rx_qenable(
	__in		efx_rxq_t *erp);

static	__checkReturn	efx_rc_t
siena_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp);

static			void
siena_rx_qdestroy(
	__in		efx_rxq_t *erp);

#endif /* EFSYS_OPT_SIENA */


#if EFSYS_OPT_SIENA
static const efx_rx_ops_t __efx_rx_siena_ops = {
	siena_rx_init,				/* erxo_init */
	siena_rx_fini,				/* erxo_fini */
#if EFSYS_OPT_RX_SCATTER
	siena_rx_scatter_enable,		/* erxo_scatter_enable */
#endif
#if EFSYS_OPT_RX_SCALE
	siena_rx_scale_mode_set,		/* erxo_scale_mode_set */
	siena_rx_scale_key_set,			/* erxo_scale_key_set */
	siena_rx_scale_tbl_set,			/* erxo_scale_tbl_set */
	siena_rx_prefix_hash,			/* erxo_prefix_hash */
#endif
	siena_rx_prefix_pktlen,			/* erxo_prefix_pktlen */
	siena_rx_qpost,				/* erxo_qpost */
	siena_rx_qpush,				/* erxo_qpush */
	siena_rx_qflush,			/* erxo_qflush */
	siena_rx_qenable,			/* erxo_qenable */
	siena_rx_qcreate,			/* erxo_qcreate */
	siena_rx_qdestroy,			/* erxo_qdestroy */
};
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD
static const efx_rx_ops_t __efx_rx_ef10_ops = {
	ef10_rx_init,				/* erxo_init */
	ef10_rx_fini,				/* erxo_fini */
#if EFSYS_OPT_RX_SCATTER
	ef10_rx_scatter_enable,			/* erxo_scatter_enable */
#endif
#if EFSYS_OPT_RX_SCALE
	ef10_rx_scale_mode_set,			/* erxo_scale_mode_set */
	ef10_rx_scale_key_set,			/* erxo_scale_key_set */
	ef10_rx_scale_tbl_set,			/* erxo_scale_tbl_set */
	ef10_rx_prefix_hash,			/* erxo_prefix_hash */
#endif
	ef10_rx_prefix_pktlen,			/* erxo_prefix_pktlen */
	ef10_rx_qpost,				/* erxo_qpost */
	ef10_rx_qpush,				/* erxo_qpush */
	ef10_rx_qflush,				/* erxo_qflush */
	ef10_rx_qenable,			/* erxo_qenable */
	ef10_rx_qcreate,			/* erxo_qcreate */
	ef10_rx_qdestroy,			/* erxo_qdestroy */
};
#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */


	__checkReturn	efx_rc_t
efx_rx_init(
	__inout		efx_nic_t *enp)
{
	const efx_rx_ops_t *erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);

	if (!(enp->en_mod_flags & EFX_MOD_EV)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_mod_flags & EFX_MOD_RX) {
		rc = EINVAL;
		goto fail2;
	}

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		erxop = &__efx_rx_siena_ops;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		erxop = &__efx_rx_ef10_ops;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		erxop = &__efx_rx_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail3;
	}

	if ((rc = erxop->erxo_init(enp)) != 0)
		goto fail4;

	enp->en_erxop = erxop;
	enp->en_mod_flags |= EFX_MOD_RX;
	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_erxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_RX;
	return (rc);
}

			void
efx_rx_fini(
	__in		efx_nic_t *enp)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_NIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);
	EFSYS_ASSERT3U(enp->en_rx_qcount, ==, 0);

	erxop->erxo_fini(enp);

	enp->en_erxop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_RX;
}

#if EFSYS_OPT_RX_SCATTER
	__checkReturn	efx_rc_t
efx_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if ((rc = erxop->erxo_scatter_enable(enp, buf_size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCATTER */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
efx_rx_hash_support_get(
	__in		efx_nic_t *enp,
	__out		efx_rx_hash_support_t *supportp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if (supportp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/* Report if resources are available to insert RX hash value */
	*supportp = enp->en_hash_support;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_rx_scale_support_get(
	__in		efx_nic_t *enp,
	__out		efx_rx_scale_support_t *supportp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if (supportp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	/* Report if resources are available to support RSS */
	*supportp = enp->en_rss_support;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if (erxop->erxo_scale_mode_set != NULL) {
		if ((rc = erxop->erxo_scale_mode_set(enp, alg,
			    type, insert)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
efx_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if ((rc = erxop->erxo_scale_key_set(enp, key, n)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_RX_SCALE
	__checkReturn	efx_rc_t
efx_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	if ((rc = erxop->erxo_scale_tbl_set(enp, table, n)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCALE */

			void
efx_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qpost(erp, addrp, size, n, completed, added);
}

			void
efx_rx_qpush(
	__in		efx_rxq_t *erp,
	__in		unsigned int added,
	__inout		unsigned int *pushedp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qpush(erp, added, pushedp);
}

	__checkReturn	efx_rc_t
efx_rx_qflush(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	if ((rc = erxop->erxo_qflush(erp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_rx_qenable(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qenable(erp);
}

	__checkReturn	efx_rc_t
efx_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__deref_out	efx_rxq_t **erpp)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;
	efx_rxq_t *erp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_RX);

	/* Allocate an RXQ object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_rxq_t), erp);

	if (erp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	erp->er_magic = EFX_RXQ_MAGIC;
	erp->er_enp = enp;
	erp->er_index = index;
	erp->er_mask = n - 1;
	erp->er_esmp = esmp;

	if ((rc = erxop->erxo_qcreate(enp, index, label, type, esmp, n, id,
	    eep, erp)) != 0)
		goto fail2;

	enp->en_rx_qcount++;
	*erpp = erp;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_rxq_t), erp);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
efx_rx_qdestroy(
	__in		efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	erxop->erxo_qdestroy(erp);
}

	__checkReturn	efx_rc_t
efx_psuedo_hdr_pkt_length_get(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;

	return (erxop->erxo_prefix_pktlen(enp, buffer, lengthp));
}

#if EFSYS_OPT_RX_SCALE
	__checkReturn	uint32_t
efx_psuedo_hdr_hash_get(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer)
{
	const efx_rx_ops_t *erxop = enp->en_erxop;

	EFSYS_ASSERT3U(enp->en_hash_support, ==, EFX_RX_HASH_AVAILABLE);
	return (erxop->erxo_prefix_hash(enp, func, buffer));
}
#endif	/* EFSYS_OPT_RX_SCALE */

#if EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
siena_rx_init(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int index;

	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_DESC_PUSH_EN, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_USR_BUF_SIZE, 0x3000 / 32);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Zero the RSS table */
	for (index = 0; index < FR_BZ_RX_INDIRECTION_TBL_ROWS;
	    index++) {
		EFX_ZERO_OWORD(oword);
		EFX_BAR_TBL_WRITEO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword, B_TRUE);
	}

#if EFSYS_OPT_RX_SCALE
	/* The RSS key and indirection table are writable. */
	enp->en_rss_support = EFX_RX_SCALE_EXCLUSIVE;

	/* Hardware can insert RX hash with/without RSS */
	enp->en_hash_support = EFX_RX_HASH_AVAILABLE;
#endif	/* EFSYS_OPT_RX_SCALE */

	return (0);
}

#if EFSYS_OPT_RX_SCATTER
static	__checkReturn	efx_rc_t
siena_rx_scatter_enable(
	__in		efx_nic_t *enp,
	__in		unsigned int buf_size)
{
	unsigned int nbuf32;
	efx_oword_t oword;
	efx_rc_t rc;

	nbuf32 = buf_size / 32;
	if ((nbuf32 == 0) ||
	    (nbuf32 >= (1 << FRF_BZ_RX_USR_BUF_SIZE_WIDTH)) ||
	    ((buf_size % 32) != 0)) {
		rc = EINVAL;
		goto fail1;
	}

	if (enp->en_rx_qcount > 0) {
		rc = EBUSY;
		goto fail2;
	}

	/* Set scatter buffer size */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_USR_BUF_SIZE, nbuf32);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* Enable scatter for packets not matching a filter */
	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_SCATTER_ENBL_NO_MATCH_Q, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_RX_SCATTER */


#define	EFX_RX_LFSR_HASH(_enp, _insert)					\
	do {								\
		efx_oword_t oword;					\
									\
		EFX_BAR_READO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP, 0);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR,	\
		    (_insert) ? 1 : 0);					\
		EFX_BAR_WRITEO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
									\
		if ((_enp)->en_family == EFX_FAMILY_SIENA) {		\
			EFX_BAR_READO((_enp), FR_CZ_RX_RSS_IPV6_REG3,	\
			    &oword);					\
			EFX_SET_OWORD_FIELD(oword,			\
			    FRF_CZ_RX_RSS_IPV6_THASH_ENABLE, 0);	\
			EFX_BAR_WRITEO((_enp), FR_CZ_RX_RSS_IPV6_REG3,	\
			    &oword);					\
		}							\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

#define	EFX_RX_TOEPLITZ_IPV4_HASH(_enp, _insert, _ip, _tcp)		\
	do {								\
		efx_oword_t oword;					\
									\
		EFX_BAR_READO((_enp), FR_AZ_RX_CFG_REG,	&oword);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_ALG, 1);	\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_IP_HASH,		\
		    (_ip) ? 1 : 0);					\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_TCP_SUP,		\
		    (_tcp) ? 0 : 1);					\
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_HASH_INSRT_HDR,	\
		    (_insert) ? 1 : 0);					\
		EFX_BAR_WRITEO((_enp), FR_AZ_RX_CFG_REG, &oword);	\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

#define	EFX_RX_TOEPLITZ_IPV6_HASH(_enp, _ip, _tcp, _rc)			\
	do {								\
		efx_oword_t oword;					\
									\
		EFX_BAR_READO((_enp), FR_CZ_RX_RSS_IPV6_REG3, &oword);	\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_THASH_ENABLE, 1);		\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_IP_THASH_ENABLE, (_ip) ? 1 : 0);	\
		EFX_SET_OWORD_FIELD(oword,				\
		    FRF_CZ_RX_RSS_IPV6_TCP_SUPPRESS, (_tcp) ? 0 : 1);	\
		EFX_BAR_WRITEO((_enp), FR_CZ_RX_RSS_IPV6_REG3, &oword);	\
									\
		(_rc) = 0;						\
									\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)


#if EFSYS_OPT_RX_SCALE

static	__checkReturn	efx_rc_t
siena_rx_scale_mode_set(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t alg,
	__in		efx_rx_hash_type_t type,
	__in		boolean_t insert)
{
	efx_rc_t rc;

	switch (alg) {
	case EFX_RX_HASHALG_LFSR:
		EFX_RX_LFSR_HASH(enp, insert);
		break;

	case EFX_RX_HASHALG_TOEPLITZ:
		EFX_RX_TOEPLITZ_IPV4_HASH(enp, insert,
		    type & (1 << EFX_RX_HASH_IPV4),
		    type & (1 << EFX_RX_HASH_TCPIPV4));

		EFX_RX_TOEPLITZ_IPV6_HASH(enp,
		    type & (1 << EFX_RX_HASH_IPV6),
		    type & (1 << EFX_RX_HASH_TCPIPV6),
		    rc);
		if (rc != 0)
			goto fail1;

		break;

	default:
		rc = EINVAL;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	EFX_RX_LFSR_HASH(enp, B_FALSE);

	return (rc);
}
#endif

#if EFSYS_OPT_RX_SCALE
static	__checkReturn	efx_rc_t
siena_rx_scale_key_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	uint8_t *key,
	__in		size_t n)
{
	efx_oword_t oword;
	unsigned int byte;
	unsigned int offset;
	efx_rc_t rc;

	byte = 0;

	/* Write Toeplitz IPv4 hash key */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_BZ_RX_RSS_TKEY_LBN + FRF_BZ_RX_RSS_TKEY_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_BZ_RX_RSS_TKEY_REG, &oword);

	byte = 0;

	/* Verify Toeplitz IPv4 hash key */
	EFX_BAR_READO(enp, FR_BZ_RX_RSS_TKEY_REG, &oword);
	for (offset = (FRF_BZ_RX_RSS_TKEY_LBN + FRF_BZ_RX_RSS_TKEY_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail1;
		}
	}

	if ((enp->en_features & EFX_FEATURE_IPV6) == 0)
		goto done;

	byte = 0;

	/* Write Toeplitz IPv6 hash key 3 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_HI_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_HI_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);

	/* Write Toeplitz IPv6 hash key 2 */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_MID_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_MID_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG2, &oword);

	/* Write Toeplitz IPv6 hash key 1 */
	EFX_ZERO_OWORD(oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_LO_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_LO_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset)
		oword.eo_u8[offset - 1] = key[byte++];

	EFX_BAR_WRITEO(enp, FR_CZ_RX_RSS_IPV6_REG1, &oword);

	byte = 0;

	/* Verify Toeplitz IPv6 hash key 3 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG3, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_HI_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_HI_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail2;
		}
	}

	/* Verify Toeplitz IPv6 hash key 2 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG2, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_MID_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_MID_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail3;
		}
	}

	/* Verify Toeplitz IPv6 hash key 1 */
	EFX_BAR_READO(enp, FR_CZ_RX_RSS_IPV6_REG1, &oword);
	for (offset = (FRF_CZ_RX_RSS_IPV6_TKEY_LO_LBN +
	    FRF_CZ_RX_RSS_IPV6_TKEY_LO_WIDTH) / 8;
	    offset > 0 && byte < n;
	    --offset) {
		if (oword.eo_u8[offset - 1] != key[byte++]) {
			rc = EFAULT;
			goto fail4;
		}
	}

done:
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
#endif

#if EFSYS_OPT_RX_SCALE
static	__checkReturn	efx_rc_t
siena_rx_scale_tbl_set(
	__in		efx_nic_t *enp,
	__in_ecount(n)	unsigned int *table,
	__in		size_t n)
{
	efx_oword_t oword;
	int index;
	efx_rc_t rc;

	EFX_STATIC_ASSERT(EFX_RSS_TBL_SIZE == FR_BZ_RX_INDIRECTION_TBL_ROWS);
	EFX_STATIC_ASSERT(EFX_MAXRSS == (1 << FRF_BZ_IT_QUEUE_WIDTH));

	if (n > FR_BZ_RX_INDIRECTION_TBL_ROWS) {
		rc = EINVAL;
		goto fail1;
	}

	for (index = 0; index < FR_BZ_RX_INDIRECTION_TBL_ROWS; index++) {
		uint32_t byte;

		/* Calculate the entry to place in the table */
		byte = (n > 0) ? (uint32_t)table[index % n] : 0;

		EFSYS_PROBE2(table, int, index, uint32_t, byte);

		EFX_POPULATE_OWORD_1(oword, FRF_BZ_IT_QUEUE, byte);

		/* Write the table */
		EFX_BAR_TBL_WRITEO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword, B_TRUE);
	}

	for (index = FR_BZ_RX_INDIRECTION_TBL_ROWS - 1; index >= 0; --index) {
		uint32_t byte;

		/* Determine if we're starting a new batch */
		byte = (n > 0) ? (uint32_t)table[index % n] : 0;

		/* Read the table */
		EFX_BAR_TBL_READO(enp, FR_BZ_RX_INDIRECTION_TBL,
				    index, &oword, B_TRUE);

		/* Verify the entry */
		if (EFX_OWORD_FIELD(oword, FRF_BZ_IT_QUEUE) != byte) {
			rc = EFAULT;
			goto fail2;
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}
#endif

/*
 * Falcon/Siena psuedo-header
 * --------------------------
 *
 * Receive packets are prefixed by an optional 16 byte pseudo-header.
 * The psuedo-header is a byte array of one of the forms:
 *
 *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.TT.TT.TT.TT
 * xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.xx.LL.LL
 *
 * where:
 *   TT.TT.TT.TT   Toeplitz hash (32-bit big-endian)
 *   LL.LL         LFSR hash     (16-bit big-endian)
 */

#if EFSYS_OPT_RX_SCALE
static	__checkReturn	uint32_t
siena_rx_prefix_hash(
	__in		efx_nic_t *enp,
	__in		efx_rx_hash_alg_t func,
	__in		uint8_t *buffer)
{
	_NOTE(ARGUNUSED(enp))

	switch (func) {
	case EFX_RX_HASHALG_TOEPLITZ:
		return ((buffer[12] << 24) |
		    (buffer[13] << 16) |
		    (buffer[14] <<  8) |
		    buffer[15]);

	case EFX_RX_HASHALG_LFSR:
		return ((buffer[14] << 8) | buffer[15]);

	default:
		EFSYS_ASSERT(0);
		return (0);
	}
}
#endif /* EFSYS_OPT_RX_SCALE */

static	__checkReturn	efx_rc_t
siena_rx_prefix_pktlen(
	__in		efx_nic_t *enp,
	__in		uint8_t *buffer,
	__out		uint16_t *lengthp)
{
	_NOTE(ARGUNUSED(enp, buffer, lengthp))

	/* Not supported by Falcon/Siena hardware */
	EFSYS_ASSERT(0);
	return (ENOTSUP);
}


static			void
siena_rx_qpost(
	__in		efx_rxq_t *erp,
	__in_ecount(n)	efsys_dma_addr_t *addrp,
	__in		size_t size,
	__in		unsigned int n,
	__in		unsigned int completed,
	__in		unsigned int added)
{
	efx_qword_t qword;
	unsigned int i;
	unsigned int offset;
	unsigned int id;

	/* The client driver must not overfill the queue */
	EFSYS_ASSERT3U(added - completed + n, <=,
	    EFX_RXQ_LIMIT(erp->er_mask + 1));

	id = added & (erp->er_mask);
	for (i = 0; i < n; i++) {
		EFSYS_PROBE4(rx_post, unsigned int, erp->er_index,
		    unsigned int, id, efsys_dma_addr_t, addrp[i],
		    size_t, size);

		EFX_POPULATE_QWORD_3(qword,
		    FSF_AZ_RX_KER_BUF_SIZE, (uint32_t)(size),
		    FSF_AZ_RX_KER_BUF_ADDR_DW0,
		    (uint32_t)(addrp[i] & 0xffffffff),
		    FSF_AZ_RX_KER_BUF_ADDR_DW1,
		    (uint32_t)(addrp[i] >> 32));

		offset = id * sizeof (efx_qword_t);
		EFSYS_MEM_WRITEQ(erp->er_esmp, offset, &qword);

		id = (id + 1) & (erp->er_mask);
	}
}

static			void
siena_rx_qpush(
	__in	efx_rxq_t *erp,
	__in	unsigned int added,
	__inout	unsigned int *pushedp)
{
	efx_nic_t *enp = erp->er_enp;
	unsigned int pushed = *pushedp;
	uint32_t wptr;
	efx_oword_t oword;
	efx_dword_t dword;

	/* All descriptors are pushed */
	*pushedp = added;

	/* Push the populated descriptors out */
	wptr = added & erp->er_mask;

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DESC_WPTR, wptr);

	/* Only write the third DWORD */
	EFX_POPULATE_DWORD_1(dword,
	    EFX_DWORD_0, EFX_OWORD_FIELD(oword, EFX_DWORD_3));

	/* Guarantee ordering of memory (descriptors) and PIO (doorbell) */
	EFX_DMA_SYNC_QUEUE_FOR_DEVICE(erp->er_esmp, erp->er_mask + 1,
	    wptr, pushed & erp->er_mask);
	EFSYS_PIO_WRITE_BARRIER();
	EFX_BAR_TBL_WRITED3(enp, FR_BZ_RX_DESC_UPD_REGP0,
			    erp->er_index, &dword, B_FALSE);
}

static	__checkReturn	efx_rc_t
siena_rx_qflush(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;
	uint32_t label;

	label = erp->er_index;

	/* Flush the queue */
	EFX_POPULATE_OWORD_2(oword, FRF_AZ_RX_FLUSH_DESCQ_CMD, 1,
	    FRF_AZ_RX_FLUSH_DESCQ, label);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FLUSH_DESCQ_REG, &oword);

	return (0);
}

static		void
siena_rx_qenable(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;

	EFSYS_ASSERT3U(erp->er_magic, ==, EFX_RXQ_MAGIC);

	EFX_BAR_TBL_READO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword, B_TRUE);

	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DC_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DESCQ_HW_RPTR, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_DESCQ_EN, 1);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword, B_TRUE);
}

static	__checkReturn	efx_rc_t
siena_rx_qcreate(
	__in		efx_nic_t *enp,
	__in		unsigned int index,
	__in		unsigned int label,
	__in		efx_rxq_type_t type,
	__in		efsys_mem_t *esmp,
	__in		size_t n,
	__in		uint32_t id,
	__in		efx_evq_t *eep,
	__in		efx_rxq_t *erp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_oword_t oword;
	uint32_t size;
	boolean_t jumbo;
	efx_rc_t rc;

	_NOTE(ARGUNUSED(esmp))

	EFX_STATIC_ASSERT(EFX_EV_RX_NLABELS ==
	    (1 << FRF_AZ_RX_DESCQ_LABEL_WIDTH));
	EFSYS_ASSERT3U(label, <, EFX_EV_RX_NLABELS);
	EFSYS_ASSERT3U(enp->en_rx_qcount + 1, <, encp->enc_rxq_limit);

	EFX_STATIC_ASSERT(ISP2(EFX_RXQ_MAXNDESCS));
	EFX_STATIC_ASSERT(ISP2(EFX_RXQ_MINNDESCS));

	if (!ISP2(n) || (n < EFX_RXQ_MINNDESCS) || (n > EFX_RXQ_MAXNDESCS)) {
		rc = EINVAL;
		goto fail1;
	}
	if (index >= encp->enc_rxq_limit) {
		rc = EINVAL;
		goto fail2;
	}
	for (size = 0; (1 << size) <= (EFX_RXQ_MAXNDESCS / EFX_RXQ_MINNDESCS);
	    size++)
		if ((1 << size) == (int)(n / EFX_RXQ_MINNDESCS))
			break;
	if (id + (1 << size) >= encp->enc_buftbl_limit) {
		rc = EINVAL;
		goto fail3;
	}

	switch (type) {
	case EFX_RXQ_TYPE_DEFAULT:
		jumbo = B_FALSE;
		break;

#if EFSYS_OPT_RX_SCATTER
	case EFX_RXQ_TYPE_SCATTER:
		if (enp->en_family < EFX_FAMILY_SIENA) {
			rc = EINVAL;
			goto fail4;
		}
		jumbo = B_TRUE;
		break;
#endif	/* EFSYS_OPT_RX_SCATTER */

	default:
		rc = EINVAL;
		goto fail4;
	}

	/* Set up the new descriptor queue */
	EFX_POPULATE_OWORD_7(oword,
	    FRF_AZ_RX_DESCQ_BUF_BASE_ID, id,
	    FRF_AZ_RX_DESCQ_EVQ_ID, eep->ee_index,
	    FRF_AZ_RX_DESCQ_OWNER_ID, 0,
	    FRF_AZ_RX_DESCQ_LABEL, label,
	    FRF_AZ_RX_DESCQ_SIZE, size,
	    FRF_AZ_RX_DESCQ_TYPE, 0,
	    FRF_AZ_RX_DESCQ_JUMBO, jumbo);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword, B_TRUE);

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

static		void
siena_rx_qdestroy(
	__in	efx_rxq_t *erp)
{
	efx_nic_t *enp = erp->er_enp;
	efx_oword_t oword;

	EFSYS_ASSERT(enp->en_rx_qcount != 0);
	--enp->en_rx_qcount;

	/* Purge descriptor queue */
	EFX_ZERO_OWORD(oword);

	EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_DESC_PTR_TBL,
			    erp->er_index, &oword, B_TRUE);

	/* Free the RXQ object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_rxq_t), erp);
}

static		void
siena_rx_fini(
	__in	efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

#endif /* EFSYS_OPT_SIENA */
