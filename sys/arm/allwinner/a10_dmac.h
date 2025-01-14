/*-
 * Copyright (c) 2014-2016 Jared D. McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/arm/allwinner/a10_dmac.h 295635 2016-02-15 19:56:35Z andrew $
 */

#ifndef _A10_DMAC_H_
#define	_A10_DMAC_H_

#define	AWIN_DMA_IRQ_EN_REG		0x0000
#define	AWIN_DMA_IRQ_PEND_STA_REG	0x0004
#define	AWIN_NDMA_AUTO_GATE_REG		0x0008
#define	AWIN_NDMA_REG(n)		(0x100+0x20*(n))
#define	AWIN_NDMA_CTL_REG		0x0000
#define	AWIN_NDMA_SRC_ADDR_REG		0x0004
#define	AWIN_NDMA_DEST_ADDR_REG		0x0008
#define	AWIN_NDMA_BC_REG		0x000c
#define	AWIN_DDMA_REG(n)		(0x300+0x20*(n))
#define	AWIN_DDMA_CTL_REG		0x0000
#define	AWIN_DDMA_SRC_START_ADDR_REG	0x0004
#define	AWIN_DDMA_DEST_START_ADDR_REG	0x0008
#define	AWIN_DDMA_BC_REG		0x000c
#define	AWIN_DDMA_PARA_REG		0x0018
#define	AWIN_DMA_IRQ_END_MASK		0xaaaaaaaa
#define	AWIN_DMA_IRQ_HF_MASK		0x55555555
#define	AWIN_DMA_IRQ_DDMA		0xffff0000
#define	AWIN_DMA_IRQ_DDMA_END(n)	(1U << (17+2*(n)))
#define	AWIN_DMA_IRQ_DDMA_HF(n)		(1U << (16+2*(n)))
#define	AWIN_DMA_IRQ_NDMA		0x0000ffff
#define	AWIN_DMA_IRQ_NDMA_END(n)	(1U << (1+2*(n)))
#define	AWIN_DMA_IRQ_NDMA_HF(n)		(1U << (0+2*(n)))
#define	AWIN_NDMA_AUTO_GATING_DIS	(1U << 16)
#define	AWIN_DMA_CTL_DST_DATA_WIDTH_SHIFT 25
#define	AWIN_DMA_CTL_DST_DATA_WIDTH_MASK (3U << AWIN_DMA_CTL_DST_DATA_WIDTH_SHIFT)
#define	AWIN_DMA_CTL_DATA_WIDTH_8	0
#define	AWIN_DMA_CTL_DATA_WIDTH_16	1
#define	AWIN_DMA_CTL_DATA_WIDTH_32	2
#define	AWIN_DMA_CTL_DST_BURST_LEN_SHIFT 23
#define	AWIN_DMA_CTL_DST_BURST_LEN_MASK	(3 << AWIN_DMA_CTL_DST_BURST_LEN_SHIFT)
#define	AWIN_DMA_CTL_BURST_LEN_1	0
#define	AWIN_DMA_CTL_BURST_LEN_4	1
#define	AWIN_DMA_CTL_BURST_LEN_8	2
#define	AWIN_DMA_CTL_DST_DRQ_TYPE_SHIFT	16
#define	AWIN_DMA_CTL_DST_DRQ_TYPE_MASK	(0x1f << AWIN_DMA_CTL_DST_DRQ_TYPE_SHIFT)
#define	AWIN_DMA_CTL_BC_REMAINING	(1U << 15)
#define	AWIN_DMA_CTL_SRC_DATA_WIDTH_SHIFT 9
#define	AWIN_DMA_CTL_SRC_DATA_WIDTH_MASK (3U << AWIN_DMA_CTL_SRC_DATA_WIDTH_SHIFT)
#define	AWIN_DMA_CTL_SRC_BURST_LEN_SHIFT 7
#define	AWIN_DMA_CTL_SRC_BURST_LEN_MASK	(3U << AWIN_DMA_CTL_SRC_BURST_LEN_SHIFT)
#define	AWIN_DMA_CTL_SRC_DRQ_TYPE_SHIFT	0
#define	AWIN_DMA_CTL_SRC_DRQ_TYPE_MASK	(0x1f << AWIN_DMA_CTL_SRC_DRQ_TYPE_SHIFT)
#define	AWIN_NDMA_CTL_DMA_LOADING	(1U << 31)
#define	AWIN_NDMA_CTL_DMA_CONTIN_MODE	(1U << 30)
#define	AWIN_NDMA_CTL_WAIT_STATE_LOG2_SHIFT 27
#define	AWIN_NDMA_CTL_WAIT_STATE_LOG2_MASK (7U << AWIN_NDMA_CTL_WAIT_STATE_LOG2_SHIFT)
#define	AWIN_NDMA_CTL_DST_NON_SECURE	(1U << 22)
#define	AWIN_NDMA_CTL_DST_ADDR_NOINCR	(1U << 21)
#define	AWIN_NDMA_CTL_DRQ_IRO		0
#define	AWIN_NDMA_CTL_DRQ_IR1		1
#define	AWIN_NDMA_CTL_DRQ_SPDIF		2
#define	AWIN_NDMA_CTL_DRQ_IISO		3
#define	AWIN_NDMA_CTL_DRQ_IIS1		4
#define	AWIN_NDMA_CTL_DRQ_AC97		5
#define	AWIN_NDMA_CTL_DRQ_IIS2		6
#define	AWIN_NDMA_CTL_DRQ_UARTO		8
#define	AWIN_NDMA_CTL_DRQ_UART1		9
#define	AWIN_NDMA_CTL_DRQ_UART2		10
#define	AWIN_NDMA_CTL_DRQ_UART3		11
#define	AWIN_NDMA_CTL_DRQ_UART4		12
#define	AWIN_NDMA_CTL_DRQ_UART5		13
#define	AWIN_NDMA_CTL_DRQ_UART6		14
#define	AWIN_NDMA_CTL_DRQ_UART7		15
#define	AWIN_NDMA_CTL_DRQ_DDC		16
#define	AWIN_NDMA_CTL_DRQ_USB_EP1	17
#define	AWIN_NDMA_CTL_DRQ_CODEC		19
#define	AWIN_NDMA_CTL_DRQ_SRAM		21
#define	AWIN_NDMA_CTL_DRQ_SDRAM		22
#define	AWIN_NDMA_CTL_DRQ_TP_AD		23
#define	AWIN_NDMA_CTL_DRQ_SPI0		24
#define	AWIN_NDMA_CTL_DRQ_SPI1		25
#define	AWIN_NDMA_CTL_DRQ_SPI2		26
#define	AWIN_NDMA_CTL_DRQ_SPI3		27
#define	AWIN_NDMA_CTL_DRQ_USB_EP2	28
#define	AWIN_NDMA_CTL_DRQ_USB_EP3	29
#define	AWIN_NDMA_CTL_DRQ_USB_EP4	30
#define	AWIN_NDMA_CTL_DRQ_USB_EP5	31
#define	AWIN_NDMA_CTL_SRC_NON_SECURE	(1U << 6)
#define	AWIN_NDMA_CTL_SRC_ADDR_NOINCR	(1U << 5)
#define	AWIN_NDMA_BC_COUNT		0x0003ffff
#define	AWIN_DDMA_CTL_DMA_LOADING	(1U << 31)
#define	AWIN_DDMA_CTL_BUSY		(1U << 30)
#define	AWIN_DDMA_CTL_DMA_CONTIN_MODE	(1U << 29)
#define	AWIN_DDMA_CTL_DST_NON_SECURE	(1U << 28)
#define	AWIN_DDMA_CTL_DST_ADDR_MODE_SHIFT 21
#define	AWIN_DDMA_CTL_DST_ADDR_MODE_MASK (3U << AWIN_DDMA_CTL_DST_ADDR_MODE_SHIFT)
#define	AWIN_DDMA_CTL_DMA_ADDR_LINEAR	0
#define	AWIN_DDMA_CTL_DMA_ADDR_IO	1
#define	AWIN_DDMA_CTL_DMA_ADDR_HPAGE	2
#define	AWIN_DDMA_CTL_DMA_ADDR_VPAGE	3
#define	AWIN_DDMA_CTL_DST_DRQ_TYPE_SHIFT 16
#define	AWIN_DDMA_CTL_DST_DRQ_TYPE_MASK	(0x1f << AWIN_DDMA_CTL_DST_DRQ_TYPE_SHIFT)
#define	AWIN_DDMA_CTL_DRQ_SRAM		0
#define	AWIN_DDMA_CTL_DRQ_SDRAM		1
#define	AWIN_DDMA_CTL_DRQ_NFC		3
#define	AWIN_DDMA_CTL_DRQ_USB0		4
#define	AWIN_DDMA_CTL_DRQ_EMAC_TX	6
#define	AWIN_DDMA_CTL_DRQ_EMAC_RX	7
#define	AWIN_DDMA_CTL_DRQ_SPI1_TX	8
#define	AWIN_DDMA_CTL_DRQ_SPI1_RX	9
#define	AWIN_DDMA_CTL_DRQ_SS_TX		10
#define	AWIN_DDMA_CTL_DRQ_SS_RX		11
#define	AWIN_DDMA_CTL_DRQ_TCON0		14
#define	AWIN_DDMA_CTL_DRQ_TCON1		15
#define	AWIN_DDMA_CTL_DRQ_MS_TX		23
#define	AWIN_DDMA_CTL_DRQ_MS_RX		23
#define	AWIN_DDMA_CTL_DRQ_HDMI_AUDIO	24
#define	AWIN_DDMA_CTL_DRQ_SPI0_TX	26
#define	AWIN_DDMA_CTL_DRQ_SPI0_RX	27
#define	AWIN_DDMA_CTL_DRQ_SPI2_TX	28
#define	AWIN_DDMA_CTL_DRQ_SPI2_RX	29
#define	AWIN_DDMA_CTL_DRQ_SPI3_TX	30
#define	AWIN_DDMA_CTL_DRQ_SPI3_RX	31
#define	AWIN_DDMA_CTL_SRC_NON_SECURE	(1U << 12)
#define	AWIN_DDMA_CTL_SRC_ADDR_MODE_SHIFT 5
#define	AWIN_DDMA_CTL_SRC_ADDR_MODE_MASK (3U << AWIN_DDMA_CTL_SRC_ADDR_MODE_SHIFT)
#define	AWIN_DDMA_BC_COUNT		0x00003fff
#define	AWIN_DDMA_PARA_DST_DATA_BLK_SIZ_SHIFT 24
#define	AWIN_DDMA_PARA_DST_DATA_BLK_SIZ_MASK (0xff << AWIN_DDMA_PARA_DST_DATA_BLK_SIZ_SHIFT)
#define	AWIN_DDMA_PARA_DST_WAIT_CYC_SHIFT 16
#define	AWIN_DDMA_PARA_DST_WAIT_CYC_MASK (0xff << AWIN_DDMA_PARA_DST_WAIT_CYC_SHIFT)
#define	AWIN_DDMA_PARA_SRC_DATA_BLK_SIZ_SHIFT 8
#define	AWIN_DDMA_PARA_SRC_DATA_BLK_SIZ_MASK (0xff << AWIN_DDMA_PARA_SRC_DATA_BLK_SIZ_SHIFT)
#define	AWIN_DDMA_PARA_SRC_WAIT_CYC_SHIFT 0
#define	AWIN_DDMA_PARA_SRC_WAIT_CYC_MASK (0xff << AWIN_DDMA_PARA_SRC_WAIT_CYC_SHIFT)

#endif /* !_A10_DMAC_H_ */
