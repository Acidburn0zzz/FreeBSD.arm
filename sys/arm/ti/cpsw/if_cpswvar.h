/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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
 * $FreeBSD: stable/11/sys/arm/ti/cpsw/if_cpswvar.h 302394 2016-07-07 20:01:03Z loos $
 */

#ifndef	_IF_CPSWVAR_H
#define	_IF_CPSWVAR_H

#define	CPSW_PORTS		2
#define	CPSW_INTR_COUNT		4

/* MII BUS  */
#define	CPSW_MIIBUS_RETRIES	5
#define	CPSW_MIIBUS_DELAY	1000

#define	CPSW_MAX_ALE_ENTRIES	1024

#define	CPSW_SYSCTL_COUNT	34

struct cpsw_slot {
	uint32_t bd_offset;  /* Offset of corresponding BD within CPPI RAM. */
	bus_dmamap_t dmamap;
	struct ifnet *ifp;
	struct mbuf *mbuf;
	STAILQ_ENTRY(cpsw_slot) next;
};
STAILQ_HEAD(cpsw_slots, cpsw_slot);

struct cpsw_queue {
	struct mtx	lock;
	int		running;
	struct cpsw_slots active;
	struct cpsw_slots avail;
	uint32_t	queue_adds; /* total bufs added */
	uint32_t	queue_removes; /* total bufs removed */
	uint32_t	queue_removes_at_last_tick; /* Used by watchdog */
	int		queue_slots;
	int		active_queue_len;
	int		max_active_queue_len;
	int		avail_queue_len;
	int		max_avail_queue_len;
	int		longest_chain; /* Largest # segments in a single packet. */
	int		hdp_offset;
};

struct cpsw_port {
	device_t	dev;
	int		phy;
	int		vlan;
};

struct cpsw_softc {
	device_t	dev;
	int		active_slave;
	int		debug;
	int		dualemac;
	phandle_t	node;
	struct bintime	attach_uptime; /* system uptime when attach happened. */
	struct cpsw_port port[2];

	/* RX and TX buffer tracking */
	struct cpsw_queue rx, tx;
	uint32_t	last_hdp;

	/* We expect 1 memory resource and 4 interrupts from the device tree. */
	int		mem_rid;
	struct resource	*mem_res;
	struct resource	*irq_res[CPSW_INTR_COUNT];
	void		*ih_cookie[CPSW_INTR_COUNT];

	/* An mbuf full of nulls for TX padding. */
	bus_dmamap_t null_mbuf_dmamap;
	struct mbuf *null_mbuf;
	bus_addr_t null_mbuf_paddr;

	bus_dma_tag_t	mbuf_dtag;

	struct {
		int resets;
		int timer;
		struct callout  callout;
	} watchdog;

	/* 64-bit versions of 32-bit hardware statistics counters */
	uint64_t shadow_stats[CPSW_SYSCTL_COUNT];

	/* CPPI STATERAM has 512 slots for building TX/RX queues. */
	/* TODO: Size here supposedly varies with different versions
	   of the controller.  Check DaVinci specs and find a good
	   way to adjust this.  One option is to have a separate
	   Device Tree parameter for number slots; another option
	   is to calculate it from the memory size in the device tree. */
	struct cpsw_slot _slots[CPSW_CPPI_RAM_SIZE / sizeof(struct cpsw_cpdma_bd)];
	struct cpsw_slots avail;
};

struct cpswp_softc {
	device_t	dev;
	device_t	miibus;
	device_t	pdev;
	int		media_status;
	int		unit;
	int		vlan;
	struct bintime	init_uptime; /* system uptime when init happened. */
	struct callout	mii_callout;
	struct cpsw_softc *swsc;
	struct ifnet	*ifp;
	struct mii_data	*mii;
	struct mtx	lock;
	uint32_t	if_flags;
	uint32_t	phy;
	uint32_t	phyaccess;
	uint32_t	physel;
};

#endif /*_IF_CPSWVAR_H */
