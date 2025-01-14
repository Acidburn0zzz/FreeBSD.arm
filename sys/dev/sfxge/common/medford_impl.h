/*-
 * Copyright (c) 2015-2016 Solarflare Communications Inc.
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
 *
 * $FreeBSD: head/sys/dev/sfxge/common/medford_impl.h 300607 2016-05-24 12:16:57Z arybchik $
 */

#ifndef	_SYS_MEDFORD_IMPL_H
#define	_SYS_MEDFORD_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

/* Alignment requirement for value written to RX WPTR:
 *  the WPTR must be aligned to an 8 descriptor boundary
 *
 * FIXME: Is this the same on Medford as Huntington?
 */
#define	MEDFORD_RX_WPTR_ALIGN	8



#ifndef	ER_EZ_TX_PIOBUF_SIZE
#define	ER_EZ_TX_PIOBUF_SIZE	4096
#endif


#define	MEDFORD_PIOBUF_NBUFS	(16)
#define	MEDFORD_PIOBUF_SIZE	(ER_EZ_TX_PIOBUF_SIZE)

#define	MEDFORD_MIN_PIO_ALLOC_SIZE	(MEDFORD_PIOBUF_SIZE / 32)


extern	__checkReturn	efx_rc_t
medford_board_cfg(
	__in		efx_nic_t *enp);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEDFORD_IMPL_H */
