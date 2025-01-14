/*-
 * Copyright (c) 2012 Semihalf.
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
 *
 * $FreeBSD: head/sys/dev/dpaa/if_dtsec_rm.h 296177 2016-02-29 03:38:00Z jhibbits $
 */

#ifndef IF_DTSEC_RM_H_
#define IF_DTSEC_RM_H_

/**
 * @group dTSEC Regular Mode API.
 * @{
 */
int	dtsec_rm_fm_port_rx_init(struct dtsec_softc *sc, int unit);
int	dtsec_rm_fm_port_tx_init(struct dtsec_softc *sc, int unit);

void	dtsec_rm_if_start_locked(struct dtsec_softc *sc);

int	dtsec_rm_pool_rx_init(struct dtsec_softc *sc);
void	dtsec_rm_pool_rx_free(struct dtsec_softc *sc);

int	dtsec_rm_fi_pool_init(struct dtsec_softc *sc);
void	dtsec_rm_fi_pool_free(struct dtsec_softc *sc);

int	dtsec_rm_fqr_rx_init(struct dtsec_softc *sc);
int	dtsec_rm_fqr_tx_init(struct dtsec_softc *sc);
void	dtsec_rm_fqr_rx_free(struct dtsec_softc *sc);
void	dtsec_rm_fqr_tx_free(struct dtsec_softc *sc);
/** @} */

#endif /* IF_DTSEC_RM_H_ */
