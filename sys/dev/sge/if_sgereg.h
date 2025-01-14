/*-
 * Copyright (c) 2008, 2009, 2010 Nikolay Denev <ndenev@gmail.com>
 * Copyright (c) 2007, 2008 Alexander Pohoyda <alexander.pohoyda@gmx.net>
 * Copyright (c) 1997, 1998, 1999
 *      Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL AUTHORS OR
 * THE VOICES IN THEIR HEADS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/dev/sge/if_sgereg.h 263957 2014-03-31 01:54:59Z yongari $
 */

#ifndef _IF_SGEREG_H
#define	_IF_SGEREG_H

/*
 * SiS PCI vendor ID.
 */
#define	SIS_VENDORID		0x1039

/*
 * SiS PCI device IDs
 */
#define	SIS_DEVICEID_190	0x0190
#define	SIS_DEVICEID_191	0x0191

#define	TX_CTL			0x00
#define	TX_DESC			0x04
#define	Reserved0		0x08
#define	TX_NEXT			0x0c

#define	RX_CTL			0x10
#define	RX_DESC			0x14
#define	Reserved1		0x18
#define	RX_NEXT			0x1c

#define	IntrStatus		0x20
#define	IntrMask		0x24
#define	IntrControl		0x28
#define	IntrTimer		0x2c

#define	PMControl		0x30
#define	Reserved2		0x34
#define	ROMControl		0x38
#define	ROMInterface		0x3c
#define	StationControl		0x40
#define	GMIIControl		0x44
#define	GMacIOCR		0x48
#define	GMacIOCTL		0x4c
#define	TxMacControl		0x50
#define	TxMacTimeLimit		0x54
#define	RGMIIDelay		0x58
#define	Reserved3		0x5c
#define	RxMacControl		0x60	/* 1  WORD */
#define	RxMacAddr		0x62	/* 6x BYTE */
#define	RxHashTable		0x68	/* 1 LONG */
#define	RxHashTable2		0x6c	/* 1 LONG */
#define	RxWakeOnLan		0x70
#define	RxWakeOnLanData		0x74
#define	RxMPSControl		0x78
#define	Reserved4		0x7c

/*
 * IntrStatus Register Content
 */
#define	INTR_SOFT		0x40000000
#define	INTR_TIMER		0x20000000
#define	INTR_PAUSE_FRAME	0x00080000
#define	INTR_MAGIC_FRAME	0x00040000
#define	INTR_WAKE_FRAME		0x00020000
#define	INTR_LINK		0x00010000
#define	INTR_RX_IDLE		0x00000080	
#define	INTR_RX_DONE		0x00000040
#define	INTR_TXQ1_IDLE		0x00000020
#define	INTR_TXQ1_DONE		0x00000010
#define	INTR_TX_IDLE		0x00000008
#define	INTR_TX_DONE		0x00000004
#define	INTR_RX_HALT		0x00000002
#define	INTR_TX_HALT		0x00000001

#define	SGE_INTRS							\
	(INTR_RX_IDLE | INTR_RX_DONE | INTR_TXQ1_IDLE |			\
	 INTR_TXQ1_DONE |INTR_TX_IDLE | INTR_TX_DONE |			\
	 INTR_TX_HALT | INTR_RX_HALT)

/*
 * RxStatusDesc Register Content
 */
#define	RxRES			0x00200000
#define	RxCRC			0x00080000
#define	RxRUNT			0x00100000
#define	RxRWT			0x00400000

/*
 * RX_CTL Register Content
 */
#define	RX_CTL_POLL		0x00000010
#define	RX_CTL_ENB		0x00000001

/*
 * TX_CTL Register Content
 */
#define	TX_CTL_POLL		0x00000010
#define	TX_CTL_ENB		0x00000001

/*
 * RxMacControl Register Content
 */
#define	AcceptBroadcast		0x0800
#define	AcceptMulticast		0x0400
#define	AcceptMyPhys		0x0200
#define	AcceptAllPhys		0x0100
#define	AcceptErr		0x0020
#define	AcceptRunt		0x0010
#define	RXMAC_STRIP_VLAN	0x0020
#define	RXMAC_STRIP_FCS		0x0010
#define	RXMAC_PAD_ENB		0x0004
#define	RXMAC_CSUM_ENB		0x0002

#define	SGE_RX_PAD_BYTES	10

/* Station control register. */
#define	SC_LOOPBACK		0x80000000
#define	SC_RGMII		0x00008000
#define	SC_FDX			0x00001000
#define	SC_SPEED_MASK		0x00000c00
#define	SC_SPEED_10		0x00000400
#define	SC_SPEED_100		0x00000800
#define	SC_SPEED_1000		0x00000c00

/*
 * Gigabit Media Independent Interface CTL register
 */
#define	GMI_DATA		0xffff0000
#define	GMI_DATA_SHIFT		16
#define	GMI_REG			0x0000f800
#define	GMI_REG_SHIFT		11
#define	GMI_PHY			0x000007c0
#define	GMI_PHY_SHIFT		6
#define	GMI_OP_WR		0x00000020
#define	GMI_OP_RD		0x00000000
#define	GMI_REQ			0x00000010
#define	GMI_MDIO		0x00000008
#define	GMI_MDDIR		0x00000004
#define	GMI_MDC			0x00000002
#define	GMI_MDEN		0x00000001

/* Tx descriptor command bits. */
#define	TDC_OWN			0x80000000
#define	TDC_INTR		0x40000000
#define	TDC_THOL3		0x30000000
#define	TDC_THOL2		0x20000000
#define	TDC_THOL1		0x10000000
#define	TDC_THOL0		0x00000000
#define	TDC_LS			0x08000000
#define	TDC_IP_CSUM		0x04000000
#define	TDC_TCP_CSUM		0x02000000
#define	TDC_UDP_CSUM		0x01000000
#define	TDC_BST			0x00800000
#define	TDC_EXT			0x00400000
#define	TDC_DEF			0x00200000
#define	TDC_BKF			0x00100000
#define	TDC_CRS			0x00080000
#define	TDC_COL			0x00040000
#define	TDC_CRC			0x00020000
#define	TDC_PAD			0x00010000
#define	TDC_VLAN_MASK		0x0000FFFF

#define	SGE_TX_INTR_FRAMES	32

/*
 * TX descriptor status bits.
 */
#define	TDS_INS_VLAN		0x80000000
#define	TDS_OWC			0x00080000
#define	TDS_ABT			0x00040000
#define	TDS_FIFO		0x00020000
#define	TDS_CRS			0x00010000
#define	TDS_COLLS		0x0000ffff
#define	SGE_TX_ERROR(x)		((x) & (TDS_OWC | TDS_ABT | TDS_FIFO | TDS_CRS))
#define	TX_ERR_BITS		"\20"				\
				"\21CRS\22FIFO\23ABT\24OWC"

/* Rx descriptor command bits. */
#define	RDC_OWN			0x80000000
#define	RDC_INTR		0x40000000
#define	RDC_IP_CSUM		0x20000000
#define	RDC_TCP_CSUM		0x10000000
#define	RDC_UDP_CSUM		0x08000000
#define	RDC_IP_CSUM_OK		0x04000000
#define	RDC_TCP_CSUM_OK		0x02000000
#define	RDC_UDP_CSUM_OK		0x01000000
#define	RDC_WAKEUP		0x00400000
#define	RDC_MAGIC		0x00200000
#define	RDC_PAUSE		0x00100000
#define	RDC_BCAST		0x000c0000
#define	RDC_MCAST		0x00080000
#define	RDC_UCAST		0x00040000
#define	RDC_CRCOFF		0x00020000
#define	RDC_PREADD		0x00010000
#define	RDC_VLAN_MASK		0x0000FFFF

/*
 * RX descriptor status bits
 */
#define	RDS_VLAN		0x80000000
#define	RDS_DESCS		0x3f000000
#define	RDS_ABORT		0x00800000
#define	RDS_SHORT		0x00400000
#define	RDS_LIMIT		0x00200000
#define	RDS_MIIER		0x00100000
#define	RDS_OVRUN		0x00080000
#define	RDS_NIBON		0x00040000
#define	RDS_COLON		0x00020000
#define	RDS_CRCOK		0x00010000
#define	SGE_RX_ERROR(x)							\
        ((x) & (RDS_COLON | RDS_NIBON | RDS_OVRUN | RDS_MIIER |		\
	RDS_LIMIT | RDS_SHORT | RDS_ABORT))
#define	SGE_RX_NSEGS(x)		(((x) & RDS_DESCS) >> 24)
#define	RX_ERR_BITS 		"\20"					\
				"\21CRCOK\22COLON\23NIBON\24OVRUN"	\
				"\25MIIER\26LIMIT\27SHORT\30ABORT"	\
				"\40VLAN"

#define	RING_END		0x80000000
#define	SGE_RX_BYTES(x)		((x) & 0xFFFF)
#define	SGE_INC(x, y)		(x) = (((x) + 1) % y)

/* Taken from Solaris driver */
#define	EI_DATA			0xffff0000
#define	EI_DATA_SHIFT		16
#define	EI_OFFSET		0x0000fc00
#define	EI_OFFSET_SHIFT		10
#define	EI_OP			0x00000300
#define	EI_OP_SHIFT		8
#define	EI_OP_RD		(2 << EI_OP_SHIFT)
#define	EI_OP_WR		(1 << EI_OP_SHIFT)
#define	EI_REQ			0x00000080
#define	EI_DO			0x00000008
#define	EI_DI			0x00000004
#define	EI_CLK			0x00000002
#define	EI_CS			0x00000001

/*
 * EEPROM Addresses
 */
#define	EEPROMSignature		0x00
#define	EEPROMCLK		0x01
#define	EEPROMInfo		0x02
#define	EEPROMMACAddr		0x03

struct sge_desc {
	uint32_t	sge_sts_size;
	uint32_t	sge_cmdsts;
	uint32_t	sge_ptr;
	uint32_t	sge_flags;
};

#define	SGE_RX_RING_CNT		256 /* [8, 1024] */
#define	SGE_TX_RING_CNT		256 /* [8, 8192] */
#define	SGE_DESC_ALIGN		16
#define	SGE_MAXTXSEGS		35
#define	SGE_TSO_MAXSIZE		(65535 + sizeof(struct ether_vlan_header))
#define	SGE_TSO_MAXSEGSIZE	4096
#define	SGE_RX_BUF_ALIGN	sizeof(uint64_t)

#define	SGE_RX_RING_SZ		(SGE_RX_RING_CNT * sizeof(struct sge_desc))
#define	SGE_TX_RING_SZ		(SGE_TX_RING_CNT * sizeof(struct sge_desc))
#define	SGE_ADDR_LO(x)		((uint64_t) (x) & 0xFFFFFFFF)

struct sge_list_data {
	struct sge_desc		*sge_rx_ring;
	struct sge_desc		*sge_tx_ring;
	/* physical bus addresses of sge_rx_ring/sge_tx_ring */
	bus_addr_t		sge_rx_paddr;
	bus_addr_t		sge_tx_paddr;
};

struct sge_txdesc {
	struct mbuf		*tx_m;
	bus_dmamap_t		tx_dmamap;
	int			tx_ndesc;
};

struct sge_rxdesc {
	struct mbuf		*rx_m;
	bus_dmamap_t		rx_dmamap;
};

struct sge_chain_data {
	bus_dma_tag_t		sge_tag;
	bus_dma_tag_t		sge_rx_tag;
	bus_dma_tag_t		sge_tx_tag;
	bus_dmamap_t		sge_rx_dmamap;
	bus_dmamap_t		sge_tx_dmamap;
	bus_dma_tag_t		sge_txmbuf_tag;
	bus_dma_tag_t		sge_rxmbuf_tag;
	struct sge_txdesc	sge_txdesc[SGE_TX_RING_CNT];
	struct sge_rxdesc	sge_rxdesc[SGE_RX_RING_CNT];
	bus_dmamap_t		sge_rx_spare_map;
	int			sge_rx_cons;
	int			sge_tx_prod;
	int			sge_tx_cons;
	int			sge_tx_cnt;
};

struct sge_type {
	uint16_t		sge_vid;
	uint16_t		sge_did;
	char			*sge_name;
};

struct sge_softc {
	struct ifnet		*sge_ifp;	/* interface info */
	struct resource		*sge_res;
	int			sge_res_id;
	int			sge_res_type;
	struct resource		*sge_irq;
	void			*sge_intrhand;
	device_t		sge_dev;
	device_t		sge_miibus;
	uint8_t			sge_rev;
	struct sge_list_data	sge_ldata;
	struct sge_chain_data	sge_cdata;
	struct callout		sge_stat_ch;
	int			sge_timer;
	int			sge_flags;
#define	SGE_FLAG_FASTETHER	0x0001
#define	SGE_FLAG_SIS190		0x0002
#define	SGE_FLAG_RGMII		0x0010
#define	SGE_FLAG_SPEED_1000	0x2000
#define	SGE_FLAG_FDX		0x4000
#define	SGE_FLAG_LINK		0x8000
	int			sge_if_flags;
	int			sge_intrcontrol;
	int			sge_intrtimer;
	struct mtx		sge_mtx;
};

#define	SGE_LOCK(_sc)		mtx_lock(&(_sc)->sge_mtx)
#define	SGE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sge_mtx)
#define	SGE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sge_mtx, MA_OWNED)

#define	SGE_TIMEOUT		1000

#endif /* _IF_SGEREG_H */
