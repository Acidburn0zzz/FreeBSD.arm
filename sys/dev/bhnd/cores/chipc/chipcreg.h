/*-
 * Copyright (c) 2015-2016 Landon Fuller <landon@landonf.org>
 * Copyright (c) 2010 Broadcom Corporation
 * All rights reserved.
 *
 * This file is derived from the sbchipc.h header distributed with
 * Broadcom's initial brcm80211 Linux driver release, as
 * contributed to the Linux staging repository.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD: head/sys/dev/bhnd/cores/chipc/chipcreg.h 302189 2016-06-25 04:33:00Z landonf $
 */

#ifndef _BHND_CORES_CHIPC_CHIPCREG_H_
#define	_BHND_CORES_CHIPC_CHIPCREG_H_

#define	CHIPC_CHIPID_SIZE		0x100	/**< size of the register block
						     containing the chip
						     identification registers
						     required during bus
						     enumeration */

/** Evaluates to true if the given ChipCommon core revision provides
 *  the core count via the chip identification register. */
#define	CHIPC_NCORES_MIN_HWREV(hwrev)	((hwrev) == 4 || (hwrev) >= 6)

#define CHIPC_GET_FLAG(_value, _flag)	(((_value) & _flag) != 0)
#define	CHIPC_GET_BITS(_value, _field)	\
	((_value & _field ## _MASK) >> _field ## _SHIFT)


#define	CHIPC_ID			0x00
#define	CHIPC_CAPABILITIES		0x04
#define	CHIPC_CORECTRL			0x08	/* rev >= 1 */
#define	CHIPC_BIST			0x0C

#define	CHIPC_OTPST			0x10	/**< otp status */
#define	CHIPC_OTPCTRL			0x14	/**< otp control */
#define	CHIPC_OTPPROG			0x18
#define	CHIPC_OTPLAYOUT			0x1C	/**< otp layout (IPX OTP) */

#define	CHIPC_INTST			0x20	/**< interrupt status */
#define	CHIPC_INTM			0x24	/**< interrupt mask */

#define	CHIPC_CHIPCTRL			0x28	/**< chip control (rev >= 11) */
#define	CHIPC_CHIPST			0x2C	/**< chip status (rev >= 11) */

#define	CHIPC_JTAGCMD			0x30
#define	CHIPC_JTAGIR			0x34
#define	CHIPC_JTAGDR			0x38
#define	CHIPC_JTAGCTRL			0x3c

#define	CHIPC_SFLASH_BASE		0x40
#define	CHIPC_SFLASH_SIZE		12
#define	CHIPC_SFLASHCTRL		0x40
#define	CHIPC_SFLASHADDR		0x44
#define	CHIPC_SFLASHDATA		0x48

/* siba backplane configuration broadcast (siba-only) */
#define	CHIPC_SBBCAST_ADDR		0x50
#define	CHIPC_SBBCAST_DATA		0x54

#define	CHIPC_GPIOPU			0x58	/**< pull-up mask (rev >= 20) */
#define	CHIPC_GPIOPD			0x5C	/**< pull down mask (rev >= 20) */
#define	CHIPC_GPIOIN			0x60
#define	CHIPC_GPIOOUT			0x64
#define	CHIPC_GPIOOUTEN			0x68
#define	CHIPC_GPIOCTRL			0x6C
#define	CHIPC_GPIOPOL			0x70
#define	CHIPC_GPIOINTM			0x74	/**< gpio interrupt mask */

#define	CHIPC_GPIOEVENT			0x78	/**< gpio event (rev >= 11) */
#define	CHIPC_GPIOEVENT_INTM		0x7C	/**< gpio event interrupt mask (rev >= 11) */

#define	CHIPC_WATCHDOG			0x80	/**< watchdog timer */

#define	CHIPC_GPIOEVENT_INTPOLARITY	0x84	/**< gpio even interrupt polarity (rev >= 11) */

#define	CHIPC_GPIOTIMERVAL		0x88	/**< gpio-based LED duty cycle (rev >= 16) */
#define	CHIPC_GPIOTIMEROUTMASK		0x8C

/* clock control block */
#define	CHIPC_CLKC_N			0x90
#define	CHIPC_CLKC_SB			0x94	/* m0 (backplane) */
#define	CHIPC_CLKC_PCI			0x98	/* m1 */
#define	CHIPC_CLKC_M2			0x9C	/* mii/uart/mipsref */
#define	CHIPC_CLKC_M3			0xA0	/* cpu */
#define	CHIPC_CLKDIV			0xA4	/* rev >= 3 */
#define	CHIPC_GPIODEBUGSEL		0xA8	/* rev >= 28 */
#define	CHIPC_CAPABILITIES_EXT		0xAC

/* pll delay (registers rev >= 4) */
#define	CHIPC_PLL_ON_DELAY		0xB0
#define	CHIPC_PLL_FREFSEL_DELAY		0xB4
#define	CHIPC_PLL_SLOWCLK_CTL		0xB8	/* revs 6-9 */

/* "instaclock" registers */
#define	CHIPC_SYS_CLK_CTL		0xC0	/* rev >= 10 */
#define	CHIPC_SYS_CLKSTATESTRETCH	0xC4	/* rev >= 10 */

/* indirect backplane access (rev >= 10) */
#define	CHIPC_BP_ADDRLOW		0xD0
#define	CHIPC_BP_ADDRHIGH		0xD4
#define	CHIPC_BP_DATA			0xD8
#define	CHIPC_BP_INDACCESS		0xE0

/* SPI/I2C (rev >= 37) */
#define	CHIPC_GSIO_CTRL			0xE4
#define	CHIPC_GSIO_ADDR			0xE8
#define	CHIPC_GSIO_DATA			0xEC

/* More clock dividers (corerev >= 32) */
#define	CHIPC_CLKDIV2			0xF0

#define	CHIPC_EROMPTR			0xFC	/**< 32-bit EROM base address
						  *  on BCMA devices */

/* ExtBus control registers (rev >= 3) */
#define	CHIPC_PCMCIA_CFG		0x100
#define	CHIPC_PCMCIA_MEMWAIT		0x104
#define	CHIPC_PCMCIA_ATTRWAIT		0x108
#define	CHIPC_PCMCIA_IOWAIT		0x10C
#define	CHIPC_IDE_CFG			0x110
#define	CHIPC_IDE_MEMWAIT		0x114
#define	CHIPC_IDE_ATTRWAIT		0x118
#define	CHIPC_IDE_IOWAIT		0x11C
#define	CHIPC_PROG_CFG			0x120
#define	CHIPC_PROG_WAITCOUNT		0x124
#define	CHIPC_FLASH_CFG			0x128
#define	CHIPC_FLASH_WAITCOUNT		0x12C
#define	CHIPC_SECI_CFG			0x130
#define	CHIPC_SECI_ST			0x134
#define	CHIPC_SECI_STM			0x138
#define	CHIPC_SECI_RXNBC		0x13C

/* Enhanced Coexistence Interface (ECI) registers (rev 21-34) */
#define	CHIPC_ECI_OUTPUT		0x140
#define	CHIPC_ECI_CTRL			0x144
#define	CHIPC_ECI_INPUTLO		0x148
#define	CHIPC_ECI_INPUTMI		0x14C
#define	CHIPC_ECI_INPUTHI		0x150
#define	CHIPC_ECI_INPUTINTPOLARITYLO	0x154
#define	CHIPC_ECI_INPUTINTPOLARITYMI	0x158
#define	CHIPC_ECI_INPUTINTPOLARITYHI	0x15C
#define	CHIPC_ECI_INTMASKLO		0x160
#define	CHIPC_ECI_INTMASKMI		0x164
#define	CHIPC_ECI_INTMASKHI		0x168
#define	CHIPC_ECI_EVENTLO		0x16C
#define	CHIPC_ECI_EVENTMI		0x170
#define	CHIPC_ECI_EVENTHI		0x174
#define	CHIPC_ECI_EVENTMASKLO		0x178
#define	CHIPC_ECI_EVENTMASKMI		0x17C
#define	CHIPC_ECI_EVENTMASKHI		0x180

#define	CHIPC_FLASHSTRCFG		0x18C	/**< BCM4706 NAND flash config */

#define	CHIPC_SPROM_CTRL		0x190	/**< SPROM interface (rev >= 32) */
#define	CHIPC_SPROM_ADDR		0x194
#define	CHIPC_SPROM_DATA		0x198

/* Clock control and hardware workarounds (corerev >= 20) */
#define	CHIPC_CLK_CTL_ST		0x1E0
#define	CHIPC_SPROM_HWWAR		0x19

#define	CHIPC_UART_BASE			0x300
#define	CHIPC_UART_SIZE			0x100
#define	CHIPC_UART_MAX			3	/**< max UART blocks */
#define	CHIPC_UART(_n)			(CHIPC_UART_BASE + (CHIPC_UART_SIZE*_n))

/* PMU registers (rev >= 20) */
#define	CHIPC_PMU_BASE			0x600
#define	CHIPC_PMU_SIZE			0x70

#define	CHIPC_PMU_CTRL			0x600
#define	CHIPC_PMU_CAP			0x604
#define	CHIPC_PMU_ST			0x608
#define	CHIPC_PMU_RES_STATE		0x60c
#define	CHIPC_PMU_RES_PENDING		0x610
#define	CHIPC_PMU_TIMER			0x614
#define	CHIPC_PMU_MIN_RES_MASK		0x618
#define	CHIPC_PMU_MAX_RES_MASK		0x61c
#define	CHIPC_PMU_RES_TABLE_SEL		0x620
#define	CHIPC_PMU_RES_DEP_MASK		0x624
#define	CHIPC_PMU_RES_UPDN_TIMER	0x628
#define	CHIPC_PMU_RES_TIMER		0x62C
#define	CHIPC_PMU_CLKSTRETCH		0x630
#define	CHIPC_PMU_WATCHDOG		0x634
#define	CHIPC_PMU_GPIOSEL		0x638	/* pmu rev >= 1 ? */
#define	CHIPC_PMU_GPIOEN		0x63C	/* pmu rev >= 1 ? */
#define	CHIPC_PMU_RES_REQ_TIMER_SEL	0x640
#define	CHIPC_PMU_RES_REQ_TIMER		0x644
#define	CHIPC_PMU_RES_REQ_MASK		0x648
#define	CHIPC_CHIPCTL_ADDR		0x650
#define	CHIPC_CHIPCTL_DATA		0x654
#define	CHIPC_PMU_REG_CONTROL_ADDR	0x658
#define	CHIPC_PMU_REG_CONTROL_DATA	0x65C
#define	CHIPC_PMU_PLL_CONTROL_ADDR 	0x660
#define	CHIPC_PMU_PLL_CONTROL_DATA 	0x664
#define	CHIPC_PMU_STRAPOPT		0x668	/* chipc rev >= 28 */
#define	CHIPC_PMU_XTALFREQ		0x66C	/* pmu rev >= 10 */

#define	CHIPC_SPROM_OTP			0x800	/* SPROM/OTP address space */
#define	CHIPC_SPROM_OTP_SIZE		0x400

/** chipid */
#define	CHIPC_ID_CHIP_MASK	0x0000FFFF	/**< chip id */
#define	CHIPC_ID_CHIP_SHIFT	0
#define	CHIPC_ID_REV_MASK	0x000F0000	/**< chip revision */
#define	CHIPC_ID_REV_SHIFT	16
#define	CHIPC_ID_PKG_MASK	0x00F00000	/**< physical package ID */
#define	CHIPC_ID_PKG_SHIFT	20
#define	CHIPC_ID_NUMCORE_MASK	0x0F000000	/**< number of cores on chip (rev >= 4) */
#define	CHIPC_ID_NUMCORE_SHIFT	24
#define	CHIPC_ID_BUS_MASK	0xF0000000	/**< chip/interconnect type (BHND_CHIPTYPE_*) */
#define	CHIPC_ID_BUS_SHIFT	28

/* capabilities */
#define	CHIPC_CAP_NUM_UART_MASK		0x00000003	/* Number of UARTs (1-3) */
#define	CHIPC_CAP_NUM_UART_SHIFT 	0
#define	CHIPC_CAP_MIPSEB		0x00000004	/* MIPS is in big-endian mode */
#define	CHIPC_CAP_UCLKSEL_MASK		0x00000018	/* UARTs clock select */
#define	CHIPC_CAP_UCLKSEL_SHIFT		3
#define	  CHIPC_CAP_UCLKSEL_UINTCLK	0x1		/* UARTs are driven by internal divided clock */
#define	CHIPC_CAP_UARTGPIO		0x00000020	/* UARTs own GPIOs 15:12 */
#define	CHIPC_CAP_EXTBUS_MASK		0x000000c0	/* External bus mask */
#define	CHIPC_CAP_EXTBUS_SHIFT		6
#define	  CHIPC_CAP_EXTBUS_NONE		0x0		/* No ExtBus present */
#define	  CHIPC_CAP_EXTBUS_FULL		0x1		/* ExtBus: PCMCIA, IDE & Prog */
#define	  CHIPC_CAP_EXTBUS_PROG		0x2		/* ExtBus: ProgIf only */
#define	CHIPC_CAP_FLASH_MASK		0x00000700	/* Type of flash */
#define	CHIPC_CAP_FLASH_SHIFT		8
#define	  CHIPC_CAP_FLASH_NONE		0x0		/* No flash */
#define	  CHIPC_CAP_SFLASH_ST		0x1		/* ST serial flash */
#define	  CHIPC_CAP_SFLASH_AT		0x2		/* Atmel serial flash */
#define	  CHIPC_CAP_NFLASH		0x3		/* NAND flash */
#define	  CHIPC_CAP_PFLASH		0x7		/* Parallel flash */
#define	CHIPC_CAP_PLL_MASK		0x00038000	/* Type of PLL */
#define	CHIPC_CAP_PLL_SHIFT		15
#define	CHIPC_CAP_PWR_CTL		0x00040000	/* Power control */
#define	CHIPC_CAP_OTP_SIZE_MASK		0x00380000	/* OTP Size (0 = none) */
#define	CHIPC_CAP_OTP_SIZE_SHIFT	19		/* OTP Size shift */
#define	CHIPC_CAP_OTP_SIZE_BASE		5		/* OTP Size base */
#define	CHIPC_CAP_JTAGP			0x00400000	/* JTAG Master Present */
#define	CHIPC_CAP_ROM			0x00800000	/* Internal boot rom active */
#define	CHIPC_CAP_BKPLN64		0x08000000	/* 64-bit backplane */
#define	CHIPC_CAP_PMU			0x10000000	/* PMU Present, rev >= 20 */
#define	CHIPC_CAP_ECI			0x20000000	/* Enhanced Coexistence Interface */
#define	CHIPC_CAP_SPROM			0x40000000	/* SPROM Present, rev >= 32 */
#define	CHIPC_CAP_4706_NFLASH		0x80000000	/* NAND flash present, BCM4706 or chipc rev38 (BCM5357)? */

#define	CHIPC_CAP2_SECI			0x00000001	/* SECI Present, rev >= 36 */
#define	CHIPC_CAP2_GSIO			0x00000002	/* GSIO (spi/i2c) present, rev >= 37 */
#define	CHIPC_CAP2_GCI			0x00000004	/* GCI present (rev >= ??) */
#define	CHIPC_CAP2_AOB			0x00000040	/* Always on Bus present (rev >= 49)
							 *
							 * If set, PMU and GCI registers
							 * are found in dedicated cores.
							 *
							 * This appears to be a lower power
							 * APB bus, bridged via ARM APB IP. */

/*
 * ChipStatus (Common)
 */

/** ChipStatus CIS/OTP/SPROM values used to advertise OTP/SPROM availability in
 *  chipcommon revs 11-31. */
enum {
	CHIPC_CST_DEFCIS_SEL	= 0,	/**< OTP is powered up, use default CIS, no SPROM */
	CHIPC_CST_SPROM_SEL	= 1,	/**< OTP is powered up, SPROM is present */
	CHIPC_CST_OTP_SEL	= 2,	/**< OTP is powered up, no SPROM */
	CHIPC_CST_OTP_PWRDN	= 3	/**< OTP is powered down, SPROM is present (rev <= 22 only) */
};


#define	CHIPC_CST_SPROM_OTP_SEL_R22_MASK	0x00000003	/**< chipstatus OTP/SPROM SEL value (rev 22) */
#define	CHIPC_CST_SPROM_OTP_SEL_R22_SHIFT	0
#define	CHIPC_CST_SPROM_OTP_SEL_R23_MASK	0x000000c0	/**< chipstatus OTP/SPROM SEL value (revs 23-31)
								  *  
								  *  it is unknown whether this is supported on
								  *  any CC revs >= 32 that also vend CHIPC_CAP_*
								  *  constants for OTP/SPROM/NVRAM availability.
								  */
#define	CHIPC_CST_SPROM_OTP_SEL_R23_SHIFT	6

/* PLL type */
#define	CHIPC_PLL_NONE		0x00000000
#define	CHIPC_PLL_TYPE1		0x00010000	/* 48MHz base, 3 dividers */
#define	CHIPC_PLL_TYPE2		0x00020000	/* 48MHz, 4 dividers */
#define	CHIPC_PLL_TYPE3		0x00030000	/* 25MHz, 2 dividers */
#define	CHIPC_PLL_TYPE4		0x00008000	/* 48MHz, 4 dividers */
#define	CHIPC_PLL_TYPE5		0x00018000	/* 25MHz, 4 dividers */
#define	CHIPC_PLL_TYPE6		0x00028000	/* 100/200 or 120/240 only */
#define	CHIPC_PLL_TYPE7		0x00038000	/* 25MHz, 4 dividers */

/* ILP clock */
#define	CHIPC_ILP_CLOCK		32000

/* ALP clock on pre-PMU chips */
#define	CHIPC_ALP_CLOCK		20000000

/* HT clock */
#define	CHIPC_HT_CLOCK		80000000

/* corecontrol */
#define	CHIPC_UARTCLKO		0x00000001	/* Drive UART with internal clock */
#define	CHIPC_SE		0x00000002	/* sync clk out enable (corerev >= 3) */
#define	CHIPC_UARTCLKEN		0x00000008	/* enable UART Clock (corerev > = 21 */

/* chipcontrol */
#define	CHIPCTRL_4321A0_DEFAULT	0x3a4
#define	CHIPCTRL_4321A1_DEFAULT	0x0a4
#define	CHIPCTRL_4321_PLL_DOWN	0x800000	/* serdes PLL down override */

/* Fields in the otpstatus register in rev >= 21 */
#define	CHIPC_OTPS_OL_MASK		0x000000ff
#define	CHIPC_OTPS_OL_MFG		0x00000001	/* manuf row is locked */
#define	CHIPC_OTPS_OL_OR1		0x00000002	/* otp redundancy row 1 is locked */
#define	CHIPC_OTPS_OL_OR2		0x00000004	/* otp redundancy row 2 is locked */
#define	CHIPC_OTPS_OL_GU		0x00000008	/* general use region is locked */
#define	CHIPC_OTPS_GUP_MASK		0x00000f00
#define	CHIPC_OTPS_GUP_SHIFT		8
#define	CHIPC_OTPS_GUP_HW		0x00000100	/* h/w subregion is programmed */
#define	CHIPC_OTPS_GUP_SW		0x00000200	/* s/w subregion is programmed */
#define	CHIPC_OTPS_GUP_CI		0x00000400	/* chipid/pkgopt subregion is programmed */
#define	CHIPC_OTPS_GUP_FUSE		0x00000800	/* fuse subregion is programmed */
#define	CHIPC_OTPS_READY		0x00001000
#define	CHIPC_OTPS_RV(x)		(1 << (16 + (x)))	/* redundancy entry valid */
#define	CHIPC_OTPS_RV_MASK		0x0fff0000

/* IPX OTP fields in the otpcontrol register */
#define	CHIPC_OTPC_PROGSEL		0x00000001
#define	CHIPC_OTPC_PCOUNT_MASK		0x0000000e
#define	CHIPC_OTPC_PCOUNT_SHIFT	1
#define	CHIPC_OTPC_VSEL_MASK		0x000000f0
#define	CHIPC_OTPC_VSEL_SHIFT		4
#define	CHIPC_OTPC_TMM_MASK		0x00000700
#define	CHIPC_OTPC_TMM_SHIFT		8
#define	CHIPC_OTPC_ODM			0x00000800
#define	CHIPC_OTPC_PROGEN		0x80000000

/* Fields in otpprog in IPX OTP and HND OTP */
#define	CHIPC_OTPP_COL_MASK		0x000000ff
#define	CHIPC_OTPP_COL_SHIFT		0
#define	CHIPC_OTPP_ROW_MASK		0x0000ff00
#define	CHIPC_OTPP_ROW_SHIFT		8
#define	CHIPC_OTPP_OC_MASK		0x0f000000
#define	CHIPC_OTPP_OC_SHIFT		24
#define	CHIPC_OTPP_READERR		0x10000000
#define	CHIPC_OTPP_VALUE_MASK		0x20000000
#define	CHIPC_OTPP_VALUE_SHIFT	29
#define	CHIPC_OTPP_START_BUSY		0x80000000
#define	CHIPC_OTPP_READ			0x40000000	/* HND OTP */

/* otplayout */
#define	CHIPC_OTPL_SIZE_MASK		0x0000f000	/* rev >= 49 */
#define	CHIPC_OTPL_SIZE_SHIFT		12
#define	CHIPC_OTPL_GUP_MASK		0x00000FFF	/* bit offset to general use region */
#define	CHIPC_OTPL_GUP_SHIFT		0
#define	CHIPC_OTPL_CISFORMAT_NEW	0x80000000	/* rev >= 36 */

/* Opcodes for OTPP_OC field */
#define	CHIPC_OTPPOC_READ		0
#define	CHIPC_OTPPOC_BIT_PROG		1
#define	CHIPC_OTPPOC_VERIFY		3
#define	CHIPC_OTPPOC_INIT		4
#define	CHIPC_OTPPOC_SET		5
#define	CHIPC_OTPPOC_RESET		6
#define	CHIPC_OTPPOC_OCST		7
#define	CHIPC_OTPPOC_ROW_LOCK		8
#define	CHIPC_OTPPOC_PRESCN_TEST	9

/* Jtagm characteristics that appeared at a given corerev */
#define	CHIPC_JTAGM_CREV_OLD		10	/* Old command set, 16bit max IR */
#define	CHIPC_JTAGM_CREV_IRP		22	/* Able to do pause-ir */
#define	CHIPC_JTAGM_CREV_RTI		28	/* Able to do return-to-idle */

/* jtagcmd */
#define	CHIPC_JCMD_START		0x80000000
#define	CHIPC_JCMD_BUSY			0x80000000
#define	CHIPC_JCMD_STATE_MASK		0x60000000
#define	CHIPC_JCMD_STATE_TLR		0x00000000	/* Test-logic-reset */
#define	CHIPC_JCMD_STATE_PIR		0x20000000	/* Pause IR */
#define	CHIPC_JCMD_STATE_PDR		0x40000000	/* Pause DR */
#define	CHIPC_JCMD_STATE_RTI		0x60000000	/* Run-test-idle */
#define	CHIPC_JCMD0_ACC_MASK		0x0000f000
#define	CHIPC_JCMD0_ACC_IRDR		0x00000000
#define	CHIPC_JCMD0_ACC_DR		0x00001000
#define	CHIPC_JCMD0_ACC_IR		0x00002000
#define	CHIPC_JCMD0_ACC_RESET		0x00003000
#define	CHIPC_JCMD0_ACC_IRPDR		0x00004000
#define	CHIPC_JCMD0_ACC_PDR		0x00005000
#define	CHIPC_JCMD0_IRW_MASK		0x00000f00
#define	CHIPC_JCMD_ACC_MASK		0x000f0000	/* Changes for corerev 11 */
#define	CHIPC_JCMD_ACC_IRDR		0x00000000
#define	CHIPC_JCMD_ACC_DR		0x00010000
#define	CHIPC_JCMD_ACC_IR		0x00020000
#define	CHIPC_JCMD_ACC_RESET		0x00030000
#define	CHIPC_JCMD_ACC_IRPDR		0x00040000
#define	CHIPC_JCMD_ACC_PDR		0x00050000
#define	CHIPC_JCMD_ACC_PIR		0x00060000
#define	CHIPC_JCMD_ACC_IRDR_I		0x00070000	/* rev 28: return to run-test-idle */
#define	CHIPC_JCMD_ACC_DR_I		0x00080000	/* rev 28: return to run-test-idle */
#define	CHIPC_JCMD_IRW_MASK		0x00001f00
#define	CHIPC_JCMD_IRW_SHIFT		8
#define	CHIPC_JCMD_DRW_MASK		0x0000003f

/* jtagctrl */
#define	CHIPC_JCTRL_FORCE_CLK		4	/* Force clock */
#define	CHIPC_JCTRL_EXT_EN		2	/* Enable external targets */
#define	CHIPC_JCTRL_EN		1	/* Enable Jtag master */

/* Fields in clkdiv */
#define	CHIPC_CLKD_SFLASH		0x0f000000
#define	CHIPC_CLKD_SFLASH_SHIFT		24
#define	CHIPC_CLKD_OTP			0x000f0000
#define	CHIPC_CLKD_OTP_SHIFT		16
#define	CHIPC_CLKD_JTAG			0x00000f00
#define	CHIPC_CLKD_JTAG_SHIFT		8
#define	CHIPC_CLKD_UART			0x000000ff

#define	CHIPC_CLKD2_SPROM		0x00000003

/* intstatus/intmask */
#define	CHIPC_CI_GPIO			0x00000001	/* gpio intr */
#define	CHIPC_CI_EI			0x00000002	/* extif intr (corerev >= 3) */
#define	CHIPC_CI_TEMP			0x00000004	/* temp. ctrl intr (corerev >= 15) */
#define	CHIPC_CI_SIRQ			0x00000008	/* serial IRQ intr (corerev >= 15) */
#define	CHIPC_CI_PMU			0x00000020	/* pmu intr (corerev >= 21) */
#define	CHIPC_CI_UART			0x00000040	/* uart intr (corerev >= 21) */
#define	CHIPC_CI_WDRESET		0x80000000	/* watchdog reset occurred */

/* slow_clk_ctl */
#define	CHIPC_SCC_SS_MASK		0x00000007	/* slow clock source mask */
#define	CHIPC_SCC_SS_LPO		0x00000000	/* source of slow clock is LPO */
#define	CHIPC_SCC_SS_XTAL		0x00000001	/* source of slow clock is crystal */
#define	CHIPC_SCC_SS_PCI		0x00000002	/* source of slow clock is PCI */
#define	CHIPC_SCC_LF			0x00000200	/* LPOFreqSel, 1: 160Khz, 0: 32KHz */
#define	CHIPC_SCC_LP			0x00000400	/* LPOPowerDown, 1: LPO is disabled,
						 * 0: LPO is enabled
						 */
#define	CHIPC_SCC_FS			0x00000800	/* ForceSlowClk, 1: sb/cores running on slow clock,
						 * 0: power logic control
						 */
#define	CHIPC_SCC_IP			0x00001000	/* IgnorePllOffReq, 1/0: power logic ignores/honors
						 * PLL clock disable requests from core
						 */
#define	CHIPC_SCC_XC			0x00002000	/* XtalControlEn, 1/0: power logic does/doesn't
						 * disable crystal when appropriate
						 */
#define	CHIPC_SCC_XP			0x00004000	/* XtalPU (RO), 1/0: crystal running/disabled */
#define	CHIPC_SCC_CD_MASK		0xffff0000	/* ClockDivider (SlowClk = 1/(4+divisor)) */
#define	CHIPC_SCC_CD_SHIFT		16

/* system_clk_ctl */
#define	CHIPC_SYCC_IE			0x00000001	/* ILPen: Enable Idle Low Power */
#define	CHIPC_SYCC_AE			0x00000002	/* ALPen: Enable Active Low Power */
#define	CHIPC_SYCC_FP			0x00000004	/* ForcePLLOn */
#define	CHIPC_SYCC_AR			0x00000008	/* Force ALP (or HT if ALPen is not set */
#define	CHIPC_SYCC_HR			0x00000010	/* Force HT */
#define	CHIPC_SYCC_CD_MASK		0xffff0000	/* ClkDiv  (ILP = 1/(4 * (divisor + 1)) */
#define	CHIPC_SYCC_CD_SHIFT		16

/* Indirect backplane access */
#define	CHIPC_BPIA_BYTEEN		0x0000000f
#define	CHIPC_BPIA_SZ1			0x00000001
#define	CHIPC_BPIA_SZ2			0x00000003
#define	CHIPC_BPIA_SZ4			0x00000007
#define	CHIPC_BPIA_SZ8			0x0000000f
#define	CHIPC_BPIA_WRITE		0x00000100
#define	CHIPC_BPIA_START		0x00000200
#define	CHIPC_BPIA_BUSY			0x00000200
#define	CHIPC_BPIA_ERROR		0x00000400

/* pcmcia/prog/flash_config */
#define	CHIPC_CF_EN			0x00000001	/* enable */
#define	CHIPC_CF_EM_MASK		0x0000000e	/* mode */
#define	CHIPC_CF_EM_SHIFT		1
#define	CHIPC_CF_EM_FLASH		0	/* flash/asynchronous mode */
#define	CHIPC_CF_EM_SYNC		2	/* synchronous mode */
#define	CHIPC_CF_EM_PCMCIA		4	/* pcmcia mode */
#define	CHIPC_CF_DS			0x00000010	/* destsize:  0=8bit, 1=16bit */
#define	CHIPC_CF_BS			0x00000020	/* byteswap */
#define	CHIPC_CF_CD_MASK		0x000000c0	/* clock divider */
#define	CHIPC_CF_CD_SHIFT		6
#define	CHIPC_CF_CD_DIV2		0x00000000	/* backplane/2 */
#define	CHIPC_CF_CD_DIV3		0x00000040	/* backplane/3 */
#define	CHIPC_CF_CD_DIV4		0x00000080	/* backplane/4 */
#define	CHIPC_CF_CE			0x00000100	/* clock enable */
#define	CHIPC_CF_SB			0x00000200	/* size/bytestrobe (synch only) */

/* pcmcia_memwait */
#define	CHIPC_PM_W0_MASK		0x0000003f	/* waitcount0 */
#define	CHIPC_PM_W1_MASK		0x00001f00	/* waitcount1 */
#define	CHIPC_PM_W1_SHIFT		8
#define	CHIPC_PM_W2_MASK		0x001f0000	/* waitcount2 */
#define	CHIPC_PM_W2_SHIFT		16
#define	CHIPC_PM_W3_MASK		0x1f000000	/* waitcount3 */
#define	CHIPC_PM_W3_SHIFT		24

/* pcmcia_attrwait */
#define	CHIPC_PA_W0_MASK		0x0000003f	/* waitcount0 */
#define	CHIPC_PA_W1_MASK		0x00001f00	/* waitcount1 */
#define	CHIPC_PA_W1_SHIFT		8
#define	CHIPC_PA_W2_MASK		0x001f0000	/* waitcount2 */
#define	CHIPC_PA_W2_SHIFT		16
#define	CHIPC_PA_W3_MASK		0x1f000000	/* waitcount3 */
#define	CHIPC_PA_W3_SHIFT		24

/* pcmcia_iowait */
#define	CHIPC_PI_W0_MASK		0x0000003f	/* waitcount0 */
#define	CHIPC_PI_W1_MASK		0x00001f00	/* waitcount1 */
#define	CHIPC_PI_W1_SHIFT		8
#define	CHIPC_PI_W2_MASK		0x001f0000	/* waitcount2 */
#define	CHIPC_PI_W2_SHIFT		16
#define	CHIPC_PI_W3_MASK		0x1f000000	/* waitcount3 */
#define	CHIPC_PI_W3_SHIFT		24

/* prog_waitcount */
#define	CHIPC_PW_W0_MASK		0x0000001f	/* waitcount0 */
#define	CHIPC_PW_W1_MASK		0x00001f00	/* waitcount1 */
#define	CHIPC_PW_W1_SHIFT		8
#define	CHIPC_PW_W2_MASK		0x001f0000	/* waitcount2 */
#define	CHIPC_PW_W2_SHIFT		16
#define	CHIPC_PW_W3_MASK		0x1f000000	/* waitcount3 */
#define	CHIPC_PW_W3_SHIFT		24

#define	CHIPC_PW_W0       		0x0000000c
#define	CHIPC_PW_W1       		0x00000a00
#define	CHIPC_PW_W2       		0x00020000
#define	CHIPC_PW_W3       		0x01000000

/* flash_waitcount */
#define	CHIPC_FW_W0_MASK		0x0000003f	/* waitcount0 */
#define	CHIPC_FW_W1_MASK		0x00001f00	/* waitcount1 */
#define	CHIPC_FW_W1_SHIFT		8
#define	CHIPC_FW_W2_MASK		0x001f0000	/* waitcount2 */
#define	CHIPC_FW_W2_SHIFT		16
#define	CHIPC_FW_W3_MASK		0x1f000000	/* waitcount3 */
#define	CHIPC_FW_W3_SHIFT		24

/* When SPROM support present, fields in spromcontrol */
#define	CHIPC_SRC_START			0x80000000
#define	CHIPC_SRC_BUSY			0x80000000
#define	CHIPC_SRC_OPCODE		0x60000000
#define	CHIPC_SRC_OP_READ		0x00000000
#define	CHIPC_SRC_OP_WRITE		0x20000000
#define	CHIPC_SRC_OP_WRDIS		0x40000000
#define	CHIPC_SRC_OP_WREN		0x60000000
#define	CHIPC_SRC_OTPSEL		0x00000010
#define	CHIPC_SRC_LOCK			0x00000008
#define	CHIPC_SRC_SIZE_MASK		0x00000006
#define	CHIPC_SRC_SIZE_1K		0x00000000
#define	CHIPC_SRC_SIZE_4K		0x00000002
#define	CHIPC_SRC_SIZE_16K		0x00000004
#define	CHIPC_SRC_SIZE_SHIFT		1
#define	CHIPC_SRC_PRESENT		0x00000001

/* Fields in pmucontrol */
#define	CHIPC_PCTL_ILP_DIV_MASK		0xffff0000
#define	CHIPC_PCTL_ILP_DIV_SHIFT	16
#define	CHIPC_PCTL_PLL_PLLCTL_UPD	0x00000400	/* rev 2 */
#define	CHIPC_PCTL_NOILP_ON_WAIT	0x00000200	/* rev 1 */
#define	CHIPC_PCTL_HT_REQ_EN		0x00000100
#define	CHIPC_PCTL_ALP_REQ_EN		0x00000080
#define	CHIPC_PCTL_XTALFREQ_MASK	0x0000007c
#define	CHIPC_PCTL_XTALFREQ_SHIFT	2
#define	CHIPC_PCTL_ILP_DIV_EN		0x00000002
#define	CHIPC_PCTL_LPO_SEL		0x00000001

/* Fields in clkstretch */
#define	CHIPC_CSTRETCH_HT		0xffff0000
#define	CHIPC_CSTRETCH_ALP		0x0000ffff

/* gpiotimerval */
#define	CHIPC_GPIO_ONTIME_SHIFT	16

/* clockcontrol_n */
#define	CHIPC_CN_N1_MASK		0x3f	/* n1 control */
#define	CHIPC_CN_N2_MASK		0x3f00	/* n2 control */
#define	CHIPC_CN_N2_SHIFT		8
#define	CHIPC_CN_PLLC_MASK		0xf0000	/* pll control */
#define	CHIPC_CN_PLLC_SHIFT		16

/* clockcontrol_sb/pci/uart */
#define	CHIPC_M1_MASK		0x3f	/* m1 control */
#define	CHIPC_M2_MASK		0x3f00	/* m2 control */
#define	CHIPC_M2_SHIFT		8
#define	CHIPC_M3_MASK		0x3f0000	/* m3 control */
#define	CHIPC_M3_SHIFT		16
#define	CHIPC_MC_MASK		0x1f000000	/* mux control */
#define	CHIPC_MC_SHIFT		24

/* N3M Clock control magic field values */
#define	CHIPC_F6_2		0x02	/* A factor of 2 in */
#define	CHIPC_F6_3		0x03	/* 6-bit fields like */
#define	CHIPC_F6_4		0x05	/* N1, M1 or M3 */
#define	CHIPC_F6_5		0x09
#define	CHIPC_F6_6		0x11
#define	CHIPC_F6_7		0x21

#define	CHIPC_F5_BIAS		5	/* 5-bit fields get this added */

#define	CHIPC_MC_BYPASS		0x08
#define	CHIPC_MC_M1		0x04
#define	CHIPC_MC_M1M2		0x02
#define	CHIPC_MC_M1M2M3		0x01
#define	CHIPC_MC_M1M3		0x11

/* Type 2 Clock control magic field values */
#define	CHIPC_T2_BIAS		2	/* n1, n2, m1 & m3 bias */
#define	CHIPC_T2M2_BIAS		3	/* m2 bias */

#define	CHIPC_T2MC_M1BYP	1
#define	CHIPC_T2MC_M2BYP	2
#define	CHIPC_T2MC_M3BYP	4

/* Type 6 Clock control magic field values */
#define	CHIPC_T6_MMASK		1	/* bits of interest in m */
#define	CHIPC_T6_M0		120000000	/* sb clock for m = 0 */
#define	CHIPC_T6_M1		100000000	/* sb clock for m = 1 */
#define	CHIPC_SB2MIPS_T6(sb)	(2 * (sb))

/* Common clock base */
#define	CHIPC_CLOCK_BASE1	24000000	/* Half the clock freq */
#define	CHIPC_CLOCK_BASE2	12500000	/* Alternate crystal on some PLLs */

/* Clock control values for 200MHz in 5350 */
#define	CHIPC_CLKC_5350_N	0x0311
#define	CHIPC_CLKC_5350_M	0x04020009

/* Bits in the ExtBus config registers */
#define	CHIPC_CFG_EN		0x0001	/* Enable */
#define	CHIPC_CFG_EM_MASK	0x000e	/* Extif Mode */
#define	CHIPC_CFG_EM_ASYNC	0x0000	/*   Async/Parallel flash */
#define	CHIPC_CFG_EM_SYNC	0x0002	/*   Synchronous */
#define	CHIPC_CFG_EM_PCMCIA	0x0004	/*   PCMCIA */
#define	CHIPC_CFG_EM_IDE	0x0006	/*   IDE */
#define	CHIPC_FLASH_CFG_DS	0x0010	/* Data size, 0=8bit, 1=16bit */
#define	CHIPC_FLASH_CFG_CD_MASK	0x00e0	/* Sync: Clock divisor, rev >= 20 */
#define	CHIPC_FLASH_CFG_CE	0x0100	/* Sync: Clock enable, rev >= 20 */
#define	CHIPC_FLASH_CFG_SB	0x0200	/* Sync: Size/Bytestrobe, rev >= 20 */
#define	CHIPC_FLASH_CFG_IS	0x0400	/* Extif Sync Clk Select, rev >= 20 */

/* ExtBus address space */
#define	CHIPC_EB_BASE		0x1a000000	/* Chipc ExtBus base address */
#define	CHIPC_EB_PCMCIA_MEM	0x1a000000	/* PCMCIA 0 memory base address */
#define	CHIPC_EB_PCMCIA_IO	0x1a200000	/* PCMCIA 0 I/O base address */
#define	CHIPC_EB_PCMCIA_CFG	0x1a400000	/* PCMCIA 0 config base address */
#define	CHIPC_EB_IDE		0x1a800000	/* IDE memory base */
#define	CHIPC_EB_PCMCIA1_MEM	0x1a800000	/* PCMCIA 1 memory base address */
#define	CHIPC_EB_PCMCIA1_IO	0x1aa00000	/* PCMCIA 1 I/O base address */
#define	CHIPC_EB_PCMCIA1_CFG	0x1ac00000	/* PCMCIA 1 config base address */
#define	CHIPC_EB_PROGIF		0x1b000000	/* ProgIF Async/Sync base address */

/* Start/busy bit in flashcontrol */
#define	CHIPC_SFLASH_OPCODE	0x000000ff
#define	CHIPC_SFLASH_ACTION	0x00000700
#define	CHIPC_SFLASH_CS_ACTIVE	0x00001000	/* Chip Select Active, rev >= 20 */
#define	CHIPC_SFLASH_START	0x80000000
#define	CHIPC_SFLASH_BUSY	SFLASH_START

/* flashcontrol action codes */
#define	CHIPC_SFLASH_ACT_OPONLY		0x0000	/* Issue opcode only */
#define	CHIPC_SFLASH_ACT_OP1D		0x0100	/* opcode + 1 data byte */
#define	CHIPC_SFLASH_ACT_OP3A		0x0200	/* opcode + 3 addr bytes */
#define	CHIPC_SFLASH_ACT_OP3A1D		0x0300	/* opcode + 3 addr & 1 data bytes */
#define	CHIPC_SFLASH_ACT_OP3A4D		0x0400	/* opcode + 3 addr & 4 data bytes */
#define	CHIPC_SFLASH_ACT_OP3A4X4D	0x0500	/* opcode + 3 addr, 4 don't care & 4 data bytes */
#define	CHIPC_SFLASH_ACT_OP3A1X4D	0x0700	/* opcode + 3 addr, 1 don't care & 4 data bytes */

/* flashcontrol action+opcodes for ST flashes */
#define	CHIPC_SFLASH_ST_WREN		0x0006	/* Write Enable */
#define	CHIPC_SFLASH_ST_WRDIS		0x0004	/* Write Disable */
#define	CHIPC_SFLASH_ST_RDSR		0x0105	/* Read Status Register */
#define	CHIPC_SFLASH_ST_WRSR		0x0101	/* Write Status Register */
#define	CHIPC_SFLASH_ST_READ		0x0303	/* Read Data Bytes */
#define	CHIPC_SFLASH_ST_PP		0x0302	/* Page Program */
#define	CHIPC_SFLASH_ST_SE		0x02d8	/* Sector Erase */
#define	CHIPC_SFLASH_ST_BE		0x00c7	/* Bulk Erase */
#define	CHIPC_SFLASH_ST_DP		0x00b9	/* Deep Power-down */
#define	CHIPC_SFLASH_ST_RES		0x03ab	/* Read Electronic Signature */
#define	CHIPC_SFLASH_ST_CSA		0x1000	/* Keep chip select asserted */
#define	CHIPC_SFLASH_ST_SSE		0x0220	/* Sub-sector Erase */

/* Status register bits for ST flashes */
#define	CHIPC_SFLASH_ST_WIP		0x01	/* Write In Progress */
#define	CHIPC_SFLASH_ST_WEL		0x02	/* Write Enable Latch */
#define	CHIPC_SFLASH_ST_BP_MASK		0x1c	/* Block Protect */
#define	CHIPC_SFLASH_ST_BP_SHIFT	2
#define	CHIPC_SFLASH_ST_SRWD		0x80	/* Status Register Write Disable */

/* flashcontrol action+opcodes for Atmel flashes */
#define	CHIPC_SFLASH_AT_READ				0x07e8
#define	CHIPC_SFLASH_AT_PAGE_READ			0x07d2
#define	CHIPC_SFLASH_AT_BUF1_READ
#define	CHIPC_SFLASH_AT_BUF2_READ
#define	CHIPC_SFLASH_AT_STATUS				0x01d7
#define	CHIPC_SFLASH_AT_BUF1_WRITE			0x0384
#define	CHIPC_SFLASH_AT_BUF2_WRITE			0x0387
#define	CHIPC_SFLASH_AT_BUF1_ERASE_PROGRAM		0x0283
#define	CHIPC_SFLASH_AT_BUF2_ERASE_PROGRAM		0x0286
#define	CHIPC_SFLASH_AT_BUF1_PROGRAM			0x0288
#define	CHIPC_SFLASH_AT_BUF2_PROGRAM			0x0289
#define	CHIPC_SFLASH_AT_PAGE_ERASE			0x0281
#define	CHIPC_SFLASH_AT_BLOCK_ERASE			0x0250
#define	CHIPC_SFLASH_AT_BUF1_WRITE_ERASE_PROGRAM	0x0382
#define	CHIPC_SFLASH_AT_BUF2_WRITE_ERASE_PROGRAM	0x0385
#define	CHIPC_SFLASH_AT_BUF1_LOAD			0x0253
#define	CHIPC_SFLASH_AT_BUF2_LOAD			0x0255
#define	CHIPC_SFLASH_AT_BUF1_COMPARE			0x0260
#define	CHIPC_SFLASH_AT_BUF2_COMPARE			0x0261
#define	CHIPC_SFLASH_AT_BUF1_REPROGRAM			0x0258
#define	CHIPC_SFLASH_AT_BUF2_REPROGRAM			0x0259

/* Status register bits for Atmel flashes */
#define	CHIPC_SFLASH_AT_READY				0x80
#define	CHIPC_SFLASH_AT_MISMATCH			0x40
#define	CHIPC_SFLASH_AT_ID_MASK				0x38
#define	CHIPC_SFLASH_AT_ID_SHIFT			3

/*
 * These are the UART port assignments, expressed as offsets from the base
 * register.  These assignments should hold for any serial port based on
 * a 8250, 16450, or 16550(A).
 */

#define	CHIPC_UART_RX			0	/* In:  Receive buffer (DLAB=0) */
#define	CHIPC_UART_TX			0	/* Out: Transmit buffer (DLAB=0) */
#define	CHIPC_UART_DLL			0	/* Out: Divisor Latch Low (DLAB=1) */
#define	CHIPC_UART_IER			1	/* In/Out: Interrupt Enable Register (DLAB=0) */
#define	CHIPC_UART_DLM			1	/* Out: Divisor Latch High (DLAB=1) */
#define	CHIPC_UART_IIR			2	/* In: Interrupt Identity Register  */
#define	CHIPC_UART_FCR			2	/* Out: FIFO Control Register */
#define	CHIPC_UART_LCR			3	/* Out: Line Control Register */
#define	CHIPC_UART_MCR			4	/* Out: Modem Control Register */
#define	CHIPC_UART_LSR			5	/* In:  Line Status Register */
#define	CHIPC_UART_MSR			6	/* In:  Modem Status Register */
#define	CHIPC_UART_SCR			7	/* I/O: Scratch Register */
#define	CHIPC_UART_LCR_DLAB		0x80	/* Divisor latch access bit */
#define	CHIPC_UART_LCR_WLEN8		0x03	/* Word length: 8 bits */
#define	CHIPC_UART_MCR_OUT2		0x08	/* MCR GPIO out 2 */
#define	CHIPC_UART_MCR_LOOP		0x10	/* Enable loopback test mode */
#define	CHIPC_UART_LSR_RX_FIFO 		0x80	/* Receive FIFO error */
#define	CHIPC_UART_LSR_TDHR		0x40	/* Data-hold-register empty */
#define	CHIPC_UART_LSR_THRE		0x20	/* Transmit-hold-register empty */
#define	CHIPC_UART_LSR_BREAK		0x10	/* Break interrupt */
#define	CHIPC_UART_LSR_FRAMING		0x08	/* Framing error */
#define	CHIPC_UART_LSR_PARITY		0x04	/* Parity error */
#define	CHIPC_UART_LSR_OVERRUN		0x02	/* Overrun error */
#define	CHIPC_UART_LSR_RXRDY		0x01	/* Receiver ready */
#define	CHIPC_UART_FCR_FIFO_ENABLE	1	/* FIFO control register bit controlling FIFO enable/disable */

/* Interrupt Identity Register (IIR) bits */
#define	CHIPC_UART_IIR_FIFO_MASK	0xc0	/* IIR FIFO disable/enabled mask */
#define	CHIPC_UART_IIR_INT_MASK		0xf	/* IIR interrupt ID source */
#define	CHIPC_UART_IIR_MDM_CHG		0x0	/* Modem status changed */
#define	CHIPC_UART_IIR_NOINT		0x1	/* No interrupt pending */
#define	CHIPC_UART_IIR_THRE		0x2	/* THR empty */
#define	CHIPC_UART_IIR_RCVD_DATA	0x4	/* Received data available */
#define	CHIPC_UART_IIR_RCVR_STATUS 	0x6	/* Receiver status */
#define	CHIPC_UART_IIR_CHAR_TIME 	0xc	/* Character time */

/* Interrupt Enable Register (IER) bits */
#define	CHIPC_UART_IER_EDSSI	8	/* enable modem status interrupt */
#define	CHIPC_UART_IER_ELSI	4	/* enable receiver line status interrupt */
#define	CHIPC_UART_IER_ETBEI	2	/* enable transmitter holding register empty interrupt */
#define	CHIPC_UART_IER_ERBFI	1	/* enable data available interrupt */

/* pmustatus */
#define	CHIPC_PST_EXTLPOAVAIL	0x0100
#define	CHIPC_PST_WDRESET	0x0080
#define	CHIPC_PST_INTPEND	0x0040
#define	CHIPC_PST_SBCLKST	0x0030
#define	CHIPC_PST_SBCLKST_ILP	0x0010
#define	CHIPC_PST_SBCLKST_ALP	0x0020
#define	CHIPC_PST_SBCLKST_HT	0x0030
#define	CHIPC_PST_ALPAVAIL	0x0008
#define	CHIPC_PST_HTAVAIL	0x0004
#define	CHIPC_PST_RESINIT	0x0003

/* pmucapabilities */
#define	CHIPC_PCAP_REV_MASK	0x000000ff
#define	CHIPC_PCAP_RC_MASK	0x00001f00
#define	CHIPC_PCAP_RC_SHIFT	8
#define	CHIPC_PCAP_TC_MASK	0x0001e000
#define	CHIPC_PCAP_TC_SHIFT	13
#define	CHIPC_PCAP_PC_MASK	0x001e0000
#define	CHIPC_PCAP_PC_SHIFT	17
#define	CHIPC_PCAP_VC_MASK	0x01e00000
#define	CHIPC_PCAP_VC_SHIFT	21
#define	CHIPC_PCAP_CC_MASK	0x1e000000
#define	CHIPC_PCAP_CC_SHIFT	25
#define	CHIPC_PCAP5_PC_MASK	0x003e0000	/* PMU corerev >= 5 */
#define	CHIPC_PCAP5_PC_SHIFT	17
#define	CHIPC_PCAP5_VC_MASK	0x07c00000
#define	CHIPC_PCAP5_VC_SHIFT	22
#define	CHIPC_PCAP5_CC_MASK	0xf8000000
#define	CHIPC_PCAP5_CC_SHIFT	27

/* PMU Resource Request Timer registers */
/* This is based on PmuRev0 */
#define	CHIPC_PRRT_TIME_MASK	0x03ff
#define	CHIPC_PRRT_INTEN	0x0400
#define	CHIPC_PRRT_REQ_ACTIVE	0x0800
#define	CHIPC_PRRT_ALP_REQ	0x1000
#define	CHIPC_PRRT_HT_REQ	0x2000

/* PMU resource bit position */
#define	CHIPC_PMURES_BIT(bit)	(1 << (bit))

/* PMU resource number limit */
#define	CHIPC_PMURES_MAX_RESNUM	30

/* PMU chip control0 register */
#define	CHIPC_PMU_CHIPCTL0	0

/* PMU chip control1 register */
#define	CHIPC_PMU_CHIPCTL1		1
#define	CHIPC_PMU_CC1_RXC_DLL_BYPASS	0x00010000

#define	CHIPC_PMU_CC1_IF_TYPE_MASK	0x00000030
#define	CHIPC_PMU_CC1_IF_TYPE_RMII	0x00000000
#define	CHIPC_PMU_CC1_IF_TYPE_MII	0x00000010
#define	CHIPC_PMU_CC1_IF_TYPE_RGMII	0x00000020

#define	CHIPC_PMU_CC1_SW_TYPE_MASK	0x000000c0
#define	CHIPC_PMU_CC1_SW_TYPE_EPHY	0x00000000
#define	CHIPC_PMU_CC1_SW_TYPE_EPHYMII	0x00000040
#define	CHIPC_PMU_CC1_SW_TYPE_EPHYRMII	0x00000080
#define	CHIPC_PMU_CC1_SW_TYPE_RGMII	0x000000c0

/* PMU corerev and chip specific PLL controls.
 * PMU<rev>_PLL<num>_XX where <rev> is PMU corerev and <num> is an arbitrary number
 * to differentiate different PLLs controlled by the same PMU rev.
 */
/* pllcontrol registers */
/* PDIV, div_phy, div_arm, div_adc, dith_sel, ioff, kpd_scale, lsb_sel, mash_sel, lf_c & lf_r */
#define	CHIPC_PMU0_PLL0_PLLCTL0			0
#define	CHIPC_PMU0_PLL0_PC0_PDIV_MASK		1
#define	CHIPC_PMU0_PLL0_PC0_PDIV_FREQ		25000
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_MASK	0x00000038
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_SHIFT	3
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_BASE	8

/* PC0_DIV_ARM for PLLOUT_ARM */
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_110MHZ	0
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_97_7MHZ	1
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_88MHZ	2
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_80MHZ	3	/* Default */
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_73_3MHZ	4
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_67_7MHZ	5
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_62_9MHZ	6
#define	CHIPC_PMU0_PLL0_PC0_DIV_ARM_58_6MHZ	7

/* Wildcard base, stop_mod, en_lf_tp, en_cal & lf_r2 */
#define	CHIPC_PMU0_PLL0_PLLCTL1			1
#define	CHIPC_PMU0_PLL0_PC1_WILD_INT_MASK	0xf0000000
#define	CHIPC_PMU0_PLL0_PC1_WILD_INT_SHIFT	28
#define	CHIPC_PMU0_PLL0_PC1_WILD_FRAC_MASK	0x0fffff00
#define	CHIPC_PMU0_PLL0_PC1_WILD_FRAC_SHIFT	8
#define	CHIPC_PMU0_PLL0_PC1_STOP_MOD		0x00000040

/* Wildcard base, vco_calvar, vco_swc, vco_var_selref, vso_ical & vco_sel_avdd */
#define	CHIPC_PMU0_PLL0_PLLCTL2			2
#define	CHIPC_PMU0_PLL0_PC2_WILD_INT_MASK	0xf
#define	CHIPC_PMU0_PLL0_PC2_WILD_INT_SHIFT	4

/* pllcontrol registers */
/* ndiv_pwrdn, pwrdn_ch<x>, refcomp_pwrdn, dly_ch<x>, p1div, p2div, _bypass_sdmod */
#define	CHIPC_PMU1_PLL0_PLLCTL0		0
#define	CHIPC_PMU1_PLL0_PC0_P1DIV_MASK	0x00f00000
#define	CHIPC_PMU1_PLL0_PC0_P1DIV_SHIFT	20
#define	CHIPC_PMU1_PLL0_PC0_P2DIV_MASK	0x0f000000
#define	CHIPC_PMU1_PLL0_PC0_P2DIV_SHIFT	24

/* m<x>div */
#define	CHIPC_PMU1_PLL0_PLLCTL1		1
#define	CHIPC_PMU1_PLL0_PC1_M1DIV_MASK	0x000000ff
#define	CHIPC_PMU1_PLL0_PC1_M1DIV_SHIFT	0
#define	CHIPC_PMU1_PLL0_PC1_M2DIV_MASK	0x0000ff00
#define	CHIPC_PMU1_PLL0_PC1_M2DIV_SHIFT	8
#define	CHIPC_PMU1_PLL0_PC1_M3DIV_MASK	0x00ff0000
#define	CHIPC_PMU1_PLL0_PC1_M3DIV_SHIFT	16
#define	CHIPC_PMU1_PLL0_PC1_M4DIV_MASK	0xff000000
#define	CHIPC_PMU1_PLL0_PC1_M4DIV_SHIFT	24

#define	CHIPC_DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT 8
#define	CHIPC_DOT11MAC_880MHZ_CLK_DIVISOR_MASK (0xFF << DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT)
#define	CHIPC_DOT11MAC_880MHZ_CLK_DIVISOR_VAL  (0xE << DOT11MAC_880MHZ_CLK_DIVISOR_SHIFT)

/* m<x>div, ndiv_dither_mfb, ndiv_mode, ndiv_int */
#define	CHIPC_PMU1_PLL0_PLLCTL2			2
#define	CHIPC_PMU1_PLL0_PC2_M5DIV_MASK		0x000000ff
#define	CHIPC_PMU1_PLL0_PC2_M5DIV_SHIFT		0
#define	CHIPC_PMU1_PLL0_PC2_M6DIV_MASK		0x0000ff00
#define	CHIPC_PMU1_PLL0_PC2_M6DIV_SHIFT		8
#define	CHIPC_PMU1_PLL0_PC2_NDIV_MODE_MASK	0x000e0000
#define	CHIPC_PMU1_PLL0_PC2_NDIV_MODE_SHIFT	17
#define	CHIPC_PMU1_PLL0_PC2_NDIV_MODE_MASH	1
#define	CHIPC_PMU1_PLL0_PC2_NDIV_MODE_MFB	2	/* recommended for 4319 */
#define	CHIPC_PMU1_PLL0_PC2_NDIV_INT_MASK	0x1ff00000
#define	CHIPC_PMU1_PLL0_PC2_NDIV_INT_SHIFT	20

/* ndiv_frac */
#define	CHIPC_PMU1_PLL0_PLLCTL3			3
#define	CHIPC_PMU1_PLL0_PC3_NDIV_FRAC_MASK	0x00ffffff
#define	CHIPC_PMU1_PLL0_PC3_NDIV_FRAC_SHIFT	0

/* pll_ctrl */
#define	CHIPC_PMU1_PLL0_PLLCTL4		4

/* pll_ctrl, vco_rng, clkdrive_ch<x> */
#define	CHIPC_PMU1_PLL0_PLLCTL5			5
#define	CHIPC_PMU1_PLL0_PC5_CLK_DRV_MASK	0xffffff00
#define	CHIPC_PMU1_PLL0_PC5_CLK_DRV_SHIFT	8

/* PMU rev 2 control words */
#define	CHIPC_PMU2_PHY_PLL_PLLCTL		4
#define	CHIPC_PMU2_SI_PLL_PLLCTL		10

/* PMU rev 2 */
/* pllcontrol registers */
/* ndiv_pwrdn, pwrdn_ch<x>, refcomp_pwrdn, dly_ch<x>, p1div, p2div, _bypass_sdmod */
#define	CHIPC_PMU2_PLL_PLLCTL0		0
#define	CHIPC_PMU2_PLL_PC0_P1DIV_MASK 	0x00f00000
#define	CHIPC_PMU2_PLL_PC0_P1DIV_SHIFT	20
#define	CHIPC_PMU2_PLL_PC0_P2DIV_MASK 	0x0f000000
#define	CHIPC_PMU2_PLL_PC0_P2DIV_SHIFT	24

/* m<x>div */
#define	CHIPC_PMU2_PLL_PLLCTL1		1
#define	CHIPC_PMU2_PLL_PC1_M1DIV_MASK 	0x000000ff
#define	CHIPC_PMU2_PLL_PC1_M1DIV_SHIFT	0
#define	CHIPC_PMU2_PLL_PC1_M2DIV_MASK 	0x0000ff00
#define	CHIPC_PMU2_PLL_PC1_M2DIV_SHIFT	8
#define	CHIPC_PMU2_PLL_PC1_M3DIV_MASK 	0x00ff0000
#define	CHIPC_PMU2_PLL_PC1_M3DIV_SHIFT	16
#define	CHIPC_PMU2_PLL_PC1_M4DIV_MASK 	0xff000000
#define	CHIPC_PMU2_PLL_PC1_M4DIV_SHIFT	24

/* m<x>div, ndiv_dither_mfb, ndiv_mode, ndiv_int */
#define	CHIPC_PMU2_PLL_PLLCTL2			2
#define	CHIPC_PMU2_PLL_PC2_M5DIV_MASK 		0x000000ff
#define	CHIPC_PMU2_PLL_PC2_M5DIV_SHIFT		0
#define	CHIPC_PMU2_PLL_PC2_M6DIV_MASK 		0x0000ff00
#define	CHIPC_PMU2_PLL_PC2_M6DIV_SHIFT		8
#define	CHIPC_PMU2_PLL_PC2_NDIV_MODE_MASK	0x000e0000
#define	CHIPC_PMU2_PLL_PC2_NDIV_MODE_SHIFT	17
#define	CHIPC_PMU2_PLL_PC2_NDIV_INT_MASK	0x1ff00000
#define	CHIPC_PMU2_PLL_PC2_NDIV_INT_SHIFT	20

/* ndiv_frac */
#define	CHIPC_PMU2_PLL_PLLCTL3			3
#define	CHIPC_PMU2_PLL_PC3_NDIV_FRAC_MASK	0x00ffffff
#define	CHIPC_PMU2_PLL_PC3_NDIV_FRAC_SHIFT	0

/* pll_ctrl */
#define	CHIPC_PMU2_PLL_PLLCTL4			4

/* pll_ctrl, vco_rng, clkdrive_ch<x> */
#define	CHIPC_PMU2_PLL_PLLCTL5			5
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH1_MASK	0x00000f00
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH1_SHIFT	8
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH2_MASK	0x0000f000
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH2_SHIFT	12
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH3_MASK	0x000f0000
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH3_SHIFT	16
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH4_MASK	0x00f00000
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH4_SHIFT	20
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH5_MASK	0x0f000000
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH5_SHIFT	24
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH6_MASK	0xf0000000
#define	CHIPC_PMU2_PLL_PC5_CLKDRIVE_CH6_SHIFT	28

/* PMU rev 5 (& 6) */
#define	CHIPC_PMU5_PLL_P1P2_OFF		0
#define	CHIPC_PMU5_PLL_P1_MASK		0x0f000000
#define	CHIPC_PMU5_PLL_P1_SHIFT		24
#define	CHIPC_PMU5_PLL_P2_MASK		0x00f00000
#define	CHIPC_PMU5_PLL_P2_SHIFT		20
#define	CHIPC_PMU5_PLL_M14_OFF		1
#define	CHIPC_PMU5_PLL_MDIV_MASK	0x000000ff
#define	CHIPC_PMU5_PLL_MDIV_WIDTH	8
#define	CHIPC_PMU5_PLL_NM5_OFF		2
#define	CHIPC_PMU5_PLL_NDIV_MASK	0xfff00000
#define	CHIPC_PMU5_PLL_NDIV_SHIFT	20
#define	CHIPC_PMU5_PLL_NDIV_MODE_MASK	0x000e0000
#define	CHIPC_PMU5_PLL_NDIV_MODE_SHIFT	17
#define	CHIPC_PMU5_PLL_FMAB_OFF		3
#define	CHIPC_PMU5_PLL_MRAT_MASK	0xf0000000
#define	CHIPC_PMU5_PLL_MRAT_SHIFT	28
#define	CHIPC_PMU5_PLL_ABRAT_MASK	0x08000000
#define	CHIPC_PMU5_PLL_ABRAT_SHIFT	27
#define	CHIPC_PMU5_PLL_FDIV_MASK	0x07ffffff
#define	CHIPC_PMU5_PLL_PLLCTL_OFF	4
#define	CHIPC_PMU5_PLL_PCHI_OFF		5
#define	CHIPC_PMU5_PLL_PCHI_MASK	0x0000003f

/* pmu XtalFreqRatio */
#define	CHIPC_PMU_XTALFREQ_REG_ILPCTR_MASK	0x00001FFF
#define	CHIPC_PMU_XTALFREQ_REG_MEASURE_MASK	0x80000000
#define	CHIPC_PMU_XTALFREQ_REG_MEASURE_SHIFT	31

/* Divider allocation in 4716/47162/5356/5357 */
#define	CHIPC_PMU5_MAINPLL_CPU		1
#define	CHIPC_PMU5_MAINPLL_MEM		2
#define	CHIPC_PMU5_MAINPLL_SI		3

#define	CHIPC_PMU7_PLL_PLLCTL7		7
#define	CHIPC_PMU7_PLL_PLLCTL8		8
#define	CHIPC_PMU7_PLL_PLLCTL11		11

/* PLL usage in 4716/47162 */
#define	CHIPC_PMU4716_MAINPLL_PLL0	12

/* PLL usage in 5356/5357 */
#define	CHIPC_PMU5356_MAINPLL_PLL0	0
#define	CHIPC_PMU5357_MAINPLL_PLL0	0

/* 4716/47162 resources */
#define	CHIPC_RES4716_PROC_PLL_ON	0x00000040
#define	CHIPC_RES4716_PROC_HT_AVAIL	0x00000080

/* 4716/4717/4718 Chip specific ChipControl register bits */
#define	CHIPC_CCTRL471X_I2S_PINS_ENABLE	0x0080	/* I2S pins off by default, shared with pflash */

/* 5354 resources */
#define	CHIPC_RES5354_EXT_SWITCHER_PWM		0	/* 0x00001 */
#define	CHIPC_RES5354_BB_SWITCHER_PWM		1	/* 0x00002 */
#define	CHIPC_RES5354_BB_SWITCHER_BURST		2	/* 0x00004 */
#define	CHIPC_RES5354_BB_EXT_SWITCHER_BURST	3	/* 0x00008 */
#define	CHIPC_RES5354_ILP_REQUEST		4	/* 0x00010 */
#define	CHIPC_RES5354_RADIO_SWITCHER_PWM	5	/* 0x00020 */
#define	CHIPC_RES5354_RADIO_SWITCHER_BURST	6	/* 0x00040 */
#define	CHIPC_RES5354_ROM_SWITCH		7	/* 0x00080 */
#define	CHIPC_RES5354_PA_REF_LDO		8	/* 0x00100 */
#define	CHIPC_RES5354_RADIO_LDO			9	/* 0x00200 */
#define	CHIPC_RES5354_AFE_LDO			10	/* 0x00400 */
#define	CHIPC_RES5354_PLL_LDO			11	/* 0x00800 */
#define	CHIPC_RES5354_BG_FILTBYP		12	/* 0x01000 */
#define	CHIPC_RES5354_TX_FILTBYP		13	/* 0x02000 */
#define	CHIPC_RES5354_RX_FILTBYP		14	/* 0x04000 */
#define	CHIPC_RES5354_XTAL_PU			15	/* 0x08000 */
#define	CHIPC_RES5354_XTAL_EN			16	/* 0x10000 */
#define	CHIPC_RES5354_BB_PLL_FILTBYP		17	/* 0x20000 */
#define	CHIPC_RES5354_RF_PLL_FILTBYP		18	/* 0x40000 */
#define	CHIPC_RES5354_BB_PLL_PU			19	/* 0x80000 */

/* 5357 Chip specific ChipControl register bits */
#define	CHIPC_CCTRL5357_EXTPA			(1<<14)	/* extPA in ChipControl 1, bit 14 */
#define	CHIPC_CCTRL5357_ANT_MUX_2o3		(1<<15)	/* 2o3 in ChipControl 1, bit 15 */

/* 4328 resources */
#define	CHIPC_RES4328_EXT_SWITCHER_PWM		0	/* 0x00001 */
#define	CHIPC_RES4328_BB_SWITCHER_PWM		1	/* 0x00002 */
#define	CHIPC_RES4328_BB_SWITCHER_BURST		2	/* 0x00004 */
#define	CHIPC_RES4328_BB_EXT_SWITCHER_BURST	3	/* 0x00008 */
#define	CHIPC_RES4328_ILP_REQUEST		4	/* 0x00010 */
#define	CHIPC_RES4328_RADIO_SWITCHER_PWM	5	/* 0x00020 */
#define	CHIPC_RES4328_RADIO_SWITCHER_BURST	6	/* 0x00040 */
#define	CHIPC_RES4328_ROM_SWITCH		7	/* 0x00080 */
#define	CHIPC_RES4328_PA_REF_LDO		8	/* 0x00100 */
#define	CHIPC_RES4328_RADIO_LDO			9	/* 0x00200 */
#define	CHIPC_RES4328_AFE_LDO			10	/* 0x00400 */
#define	CHIPC_RES4328_PLL_LDO			11	/* 0x00800 */
#define	CHIPC_RES4328_BG_FILTBYP		12	/* 0x01000 */
#define	CHIPC_RES4328_TX_FILTBYP		13	/* 0x02000 */
#define	CHIPC_RES4328_RX_FILTBYP		14	/* 0x04000 */
#define	CHIPC_RES4328_XTAL_PU			15	/* 0x08000 */
#define	CHIPC_RES4328_XTAL_EN			16	/* 0x10000 */
#define	CHIPC_RES4328_BB_PLL_FILTBYP		17	/* 0x20000 */
#define	CHIPC_RES4328_RF_PLL_FILTBYP		18	/* 0x40000 */
#define	CHIPC_RES4328_BB_PLL_PU			19	/* 0x80000 */

/* 4325 A0/A1 resources */
#define	CHIPC_RES4325_BUCK_BOOST_BURST		0	/* 0x00000001 */
#define	CHIPC_RES4325_CBUCK_BURST		1	/* 0x00000002 */
#define	CHIPC_RES4325_CBUCK_PWM			2	/* 0x00000004 */
#define	CHIPC_RES4325_CLDO_CBUCK_BURST		3	/* 0x00000008 */
#define	CHIPC_RES4325_CLDO_CBUCK_PWM		4	/* 0x00000010 */
#define	CHIPC_RES4325_BUCK_BOOST_PWM		5	/* 0x00000020 */
#define	CHIPC_RES4325_ILP_REQUEST		6	/* 0x00000040 */
#define	CHIPC_RES4325_ABUCK_BURST		7	/* 0x00000080 */
#define	CHIPC_RES4325_ABUCK_PWM			8	/* 0x00000100 */
#define	CHIPC_RES4325_LNLDO1_PU			9	/* 0x00000200 */
#define	CHIPC_RES4325_OTP_PU			10	/* 0x00000400 */
#define	CHIPC_RES4325_LNLDO3_PU			11	/* 0x00000800 */
#define	CHIPC_RES4325_LNLDO4_PU			12	/* 0x00001000 */
#define	CHIPC_RES4325_XTAL_PU			13	/* 0x00002000 */
#define	CHIPC_RES4325_ALP_AVAIL			14	/* 0x00004000 */
#define	CHIPC_RES4325_RX_PWRSW_PU		15	/* 0x00008000 */
#define	CHIPC_RES4325_TX_PWRSW_PU		16	/* 0x00010000 */
#define	CHIPC_RES4325_RFPLL_PWRSW_PU		17	/* 0x00020000 */
#define	CHIPC_RES4325_LOGEN_PWRSW_PU		18	/* 0x00040000 */
#define	CHIPC_RES4325_AFE_PWRSW_PU		19	/* 0x00080000 */
#define	CHIPC_RES4325_BBPLL_PWRSW_PU		20	/* 0x00100000 */
#define	CHIPC_RES4325_HT_AVAIL			21	/* 0x00200000 */

/* 4325 B0/C0 resources */
#define	CHIPC_RES4325B0_CBUCK_LPOM		1	/* 0x00000002 */
#define	CHIPC_RES4325B0_CBUCK_BURST		2	/* 0x00000004 */
#define	CHIPC_RES4325B0_CBUCK_PWM		3	/* 0x00000008 */
#define	CHIPC_RES4325B0_CLDO_PU			4	/* 0x00000010 */

/* 4325 C1 resources */
#define	CHIPC_RES4325C1_LNLDO2_PU		12	/* 0x00001000 */

/* 4325 chip-specific ChipStatus register bits */
#define	CHIPC_CST4325_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R22_MASK
#define	CHIPC_CST4325_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R22_SHIFT
#define	CHIPC_CST4325_SDIO_USB_MODE_MASK	0x00000004
#define	CHIPC_CST4325_SDIO_USB_MODE_SHIFT	2
#define	CHIPC_CST4325_RCAL_VALID_MASK		0x00000008
#define	CHIPC_CST4325_RCAL_VALID_SHIFT		3
#define	CHIPC_CST4325_RCAL_VALUE_MASK		0x000001f0
#define	CHIPC_CST4325_RCAL_VALUE_SHIFT		4
#define	CHIPC_CST4325_PMUTOP_2B_MASK 		0x00000200	/* 1 for 2b, 0 for to 2a */
#define	CHIPC_CST4325_PMUTOP_2B_SHIFT   	9

#define	CHIPC_RES4329_RESERVED0			0	/* 0x00000001 */
#define	CHIPC_RES4329_CBUCK_LPOM		1	/* 0x00000002 */
#define	CHIPC_RES4329_CBUCK_BURST		2	/* 0x00000004 */
#define	CHIPC_RES4329_CBUCK_PWM			3	/* 0x00000008 */
#define	CHIPC_RES4329_CLDO_PU			4	/* 0x00000010 */
#define	CHIPC_RES4329_PALDO_PU			5	/* 0x00000020 */
#define	CHIPC_RES4329_ILP_REQUEST		6	/* 0x00000040 */
#define	CHIPC_RES4329_RESERVED7			7	/* 0x00000080 */
#define	CHIPC_RES4329_RESERVED8			8	/* 0x00000100 */
#define	CHIPC_RES4329_LNLDO1_PU			9	/* 0x00000200 */
#define	CHIPC_RES4329_OTP_PU			10	/* 0x00000400 */
#define	CHIPC_RES4329_RESERVED11		11	/* 0x00000800 */
#define	CHIPC_RES4329_LNLDO2_PU			12	/* 0x00001000 */
#define	CHIPC_RES4329_XTAL_PU			13	/* 0x00002000 */
#define	CHIPC_RES4329_ALP_AVAIL			14	/* 0x00004000 */
#define	CHIPC_RES4329_RX_PWRSW_PU		15	/* 0x00008000 */
#define	CHIPC_RES4329_TX_PWRSW_PU		16	/* 0x00010000 */
#define	CHIPC_RES4329_RFPLL_PWRSW_PU		17	/* 0x00020000 */
#define	CHIPC_RES4329_LOGEN_PWRSW_PU		18	/* 0x00040000 */
#define	CHIPC_RES4329_AFE_PWRSW_PU		19	/* 0x00080000 */
#define	CHIPC_RES4329_BBPLL_PWRSW_PU		20	/* 0x00100000 */
#define	CHIPC_RES4329_HT_AVAIL			21	/* 0x00200000 */

/* 4329 chip-specific ChipStatus register bits */
#define	CHIPC_CST4329_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R22_MASK
#define	CHIPC_CST4329_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R22_SHIFT
#define	CHIPC_CST4329_SPI_SDIO_MODE_MASK	0x00000004
#define	CHIPC_CST4329_SPI_SDIO_MODE_SHIFT	2

/* 4312 chip-specific ChipStatus register bits */
#define	CHIPC_CST4312_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R22_MASK
#define	CHIPC_CST4312_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R22_SHIFT

/* 4312 resources (all PMU chips with little memory constraint) */
#define	CHIPC_RES4312_SWITCHER_BURST		0	/* 0x00000001 */
#define	CHIPC_RES4312_SWITCHER_PWM    		1	/* 0x00000002 */
#define	CHIPC_RES4312_PA_REF_LDO		2	/* 0x00000004 */
#define	CHIPC_RES4312_CORE_LDO_BURST		3	/* 0x00000008 */
#define	CHIPC_RES4312_CORE_LDO_PWM		4	/* 0x00000010 */
#define	CHIPC_RES4312_RADIO_LDO			5	/* 0x00000020 */
#define	CHIPC_RES4312_ILP_REQUEST		6	/* 0x00000040 */
#define	CHIPC_RES4312_BG_FILTBYP		7	/* 0x00000080 */
#define	CHIPC_RES4312_TX_FILTBYP		8	/* 0x00000100 */
#define	CHIPC_RES4312_RX_FILTBYP		9	/* 0x00000200 */
#define	CHIPC_RES4312_XTAL_PU			10	/* 0x00000400 */
#define	CHIPC_RES4312_ALP_AVAIL			11	/* 0x00000800 */
#define	CHIPC_RES4312_BB_PLL_FILTBYP		12	/* 0x00001000 */
#define	CHIPC_RES4312_RF_PLL_FILTBYP		13	/* 0x00002000 */
#define	CHIPC_RES4312_HT_AVAIL			14	/* 0x00004000 */

/* 4322 resources */
#define	CHIPC_RES4322_RF_LDO			0
#define	CHIPC_RES4322_ILP_REQUEST		1
#define	CHIPC_RES4322_XTAL_PU			2
#define	CHIPC_RES4322_ALP_AVAIL			3
#define	CHIPC_RES4322_SI_PLL_ON			4
#define	CHIPC_RES4322_HT_SI_AVAIL		5
#define	CHIPC_RES4322_PHY_PLL_ON		6
#define	CHIPC_RES4322_HT_PHY_AVAIL		7
#define	CHIPC_RES4322_OTP_PU			8

/* 4322 chip-specific ChipStatus register bits */
#define	CHIPC_CST4322_XTAL_FREQ_20_40MHZ	0x00000020
#define	CHIPC_CST4322_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R23_MASK
#define	CHIPC_CST4322_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R23_SHIFT
#define	CHIPC_CST4322_PCI_OR_USB		0x00000100
#define	CHIPC_CST4322_BOOT_MASK			0x00000600
#define	CHIPC_CST4322_BOOT_SHIFT		9
#define	CHIPC_CST4322_BOOT_FROM_SRAM		0	/* boot from SRAM, ARM in reset */
#define	CHIPC_CST4322_BOOT_FROM_ROM		1	/* boot from ROM */
#define	CHIPC_CST4322_BOOT_FROM_FLASH		2	/* boot from FLASH */
#define	CHIPC_CST4322_BOOT_FROM_INVALID		3
#define	CHIPC_CST4322_ILP_DIV_EN		0x00000800
#define	CHIPC_CST4322_FLASH_TYPE_MASK		0x00001000
#define	CHIPC_CST4322_FLASH_TYPE_SHIFT		12
#define	CHIPC_CST4322_FLASH_TYPE_SHIFT_ST	0	/* ST serial FLASH */
#define	CHIPC_CST4322_FLASH_TYPE_SHIFT_ATMEL	1	/* ATMEL flash */
#define	CHIPC_CST4322_ARM_TAP_SEL		0x00002000
#define	CHIPC_CST4322_RES_INIT_MODE_MASK	0x0000c000
#define	CHIPC_CST4322_RES_INIT_MODE_SHIFT	14
#define	CHIPC_CST4322_RES_INIT_MODE_ILPAVAIL	0	/* resinitmode: ILP available */
#define	CHIPC_CST4322_RES_INIT_MODE_ILPREQ	1	/* resinitmode: ILP request */
#define	CHIPC_CST4322_RES_INIT_MODE_ALPAVAIL	2	/* resinitmode: ALP available */
#define	CHIPC_CST4322_RES_INIT_MODE_HTAVAIL	3	/* resinitmode: HT available */
#define	CHIPC_CST4322_PCIPLLCLK_GATING		0x00010000
#define	CHIPC_CST4322_CLK_SWITCH_PCI_TO_ALP	0x00020000
#define	CHIPC_CST4322_PCI_CARDBUS_MODE		0x00040000

/* 43224 chip-specific ChipControl register bits */
#define	CHIPC_CCTRL43224_GPIO_TOGGLE		0x8000
#define	CHIPC_CCTRL_43224A0_12MA_LED_DRIVE	0x00F000F0	/* 12 mA drive strength */
#define	CHIPC_CCTRL_43224B0_12MA_LED_DRIVE	0xF0	/* 12 mA drive strength for later 43224s */

/* 43236 resources */
#define	CHIPC_RES43236_REGULATOR		0
#define	CHIPC_RES43236_ILP_REQUEST		1
#define	CHIPC_RES43236_XTAL_PU			2
#define	CHIPC_RES43236_ALP_AVAIL		3
#define	CHIPC_RES43236_SI_PLL_ON		4
#define	CHIPC_RES43236_HT_SI_AVAIL		5

/* 43236 chip-specific ChipControl register bits */
#define	CHIPC_CCTRL43236_BT_COEXIST		(1<<0)	/* 0 disable */
#define	CHIPC_CCTRL43236_SECI			(1<<1)	/* 0 SECI is disabled (JATG functional) */
#define	CHIPC_CCTRL43236_EXT_LNA		(1<<2)	/* 0 disable */
#define	CHIPC_CCTRL43236_ANT_MUX_2o3		(1<<3)	/* 2o3 mux, chipcontrol bit 3 */
#define	CHIPC_CCTRL43236_GSIO			(1<<4)	/* 0 disable */

/* 43236 Chip specific ChipStatus register bits */
#define	CHIPC_CST43236_SFLASH_MASK		0x00000040
#define	CHIPC_CST43236_OTP_SEL_MASK		0x00000080
#define	CHIPC_CST43236_OTP_SEL_SHIFT		7
#define	CHIPC_CST43236_HSIC_MASK		0x00000100	/* USB/HSIC */
#define	CHIPC_CST43236_BP_CLK			0x00000200	/* 120/96Mbps */
#define	CHIPC_CST43236_BOOT_MASK		0x00001800
#define	CHIPC_CST43236_BOOT_SHIFT		11
#define	CHIPC_CST43236_BOOT_FROM_SRAM		0	/* boot from SRAM, ARM in reset */
#define	CHIPC_CST43236_BOOT_FROM_ROM		1	/* boot from ROM */
#define	CHIPC_CST43236_BOOT_FROM_FLASH		2	/* boot from FLASH */
#define	CHIPC_CST43236_BOOT_FROM_INVALID	3

/* 4331 resources */
#define	CHIPC_RES4331_REGULATOR			0
#define	CHIPC_RES4331_ILP_REQUEST		1
#define	CHIPC_RES4331_XTAL_PU			2
#define	CHIPC_RES4331_ALP_AVAIL			3
#define	CHIPC_RES4331_SI_PLL_ON			4
#define	CHIPC_RES4331_HT_SI_AVAIL		5

/* 4331 chip-specific ChipControl register bits */
#define	CHIPC_CCTRL4331_BT_COEXIST		(1<<0)	/* 0 disable */
#define	CHIPC_CCTRL4331_SECI			(1<<1)	/* 0 SECI is disabled (JATG functional) */
#define	CHIPC_CCTRL4331_EXT_LNA			(1<<2)	/* 0 disable */
#define	CHIPC_CCTRL4331_SPROM_GPIO13_15		(1<<3)	/* sprom/gpio13-15 mux */
#define	CHIPC_CCTRL4331_EXTPA_EN		(1<<4)	/* 0 ext pa disable, 1 ext pa enabled */
#define	CHIPC_CCTRL4331_GPIOCLK_ON_SPROMCS	(1<<5)	/* set drive out GPIO_CLK on sprom_cs pin */
#define	CHIPC_CCTRL4331_PCIE_MDIO_ON_SPROMCS	(1<<6)	/* use sprom_cs pin as PCIE mdio interface */
#define	CHIPC_CCTRL4331_EXTPA_ON_GPIO2_5	(1<<7)	/* aband extpa will be at gpio2/5 and sprom_dout */
#define	CHIPC_CCTRL4331_OVR_PIPEAUXCLKEN	(1<<8)	/* override core control on pipe_AuxClkEnable */
#define	CHIPC_CCTRL4331_OVR_PIPEAUXPWRDOWN	(1<<9)	/* override core control on pipe_AuxPowerDown */
#define	CHIPC_CCTRL4331_PCIE_AUXCLKEN		(1<<10)	/* pcie_auxclkenable */
#define	CHIPC_CCTRL4331_PCIE_PIPE_PLLDOWN	(1<<11)	/* pcie_pipe_pllpowerdown */
#define	CHIPC_CCTRL4331_EXTPA_EN2		(1<<12)	/* 0 ext pa2 disable, 1 ext pa2 enabled */
#define	CHIPC_CCTRL4331_BT_SHD0_ON_GPIO4	(1<<16)	/* enable bt_shd0 at gpio4 */
#define	CHIPC_CCTRL4331_BT_SHD1_ON_GPIO5	(1<<17)	/* enable bt_shd1 at gpio5 */

/* 4331 Chip specific ChipStatus register bits */
#define	CHIPC_CST4331_XTAL_FREQ			0x00000001	/* crystal frequency 20/40Mhz */
#define	CHIPC_CST4331_SPROM_PRESENT		0x00000002
#define	CHIPC_CST4331_OTP_PRESENT		0x00000004
#define	CHIPC_CST4331_LDO_RF			0x00000008
#define	CHIPC_CST4331_LDO_PAR			0x00000010

/* 4315 resources */
#define	CHIPC_RES4315_CBUCK_LPOM		1	/* 0x00000002 */
#define	CHIPC_RES4315_CBUCK_BURST		2	/* 0x00000004 */
#define	CHIPC_RES4315_CBUCK_PWM			3	/* 0x00000008 */
#define	CHIPC_RES4315_CLDO_PU			4	/* 0x00000010 */
#define	CHIPC_RES4315_PALDO_PU			5	/* 0x00000020 */
#define	CHIPC_RES4315_ILP_REQUEST		6	/* 0x00000040 */
#define	CHIPC_RES4315_LNLDO1_PU			9	/* 0x00000200 */
#define	CHIPC_RES4315_OTP_PU			10	/* 0x00000400 */
#define	CHIPC_RES4315_LNLDO2_PU			12	/* 0x00001000 */
#define	CHIPC_RES4315_XTAL_PU			13	/* 0x00002000 */
#define	CHIPC_RES4315_ALP_AVAIL			14	/* 0x00004000 */
#define	CHIPC_RES4315_RX_PWRSW_PU		15	/* 0x00008000 */
#define	CHIPC_RES4315_TX_PWRSW_PU		16	/* 0x00010000 */
#define	CHIPC_RES4315_RFPLL_PWRSW_PU		17	/* 0x00020000 */
#define	CHIPC_RES4315_LOGEN_PWRSW_PU		18	/* 0x00040000 */
#define	CHIPC_RES4315_AFE_PWRSW_PU		19	/* 0x00080000 */
#define	CHIPC_RES4315_BBPLL_PWRSW_PU		20	/* 0x00100000 */
#define	CHIPC_RES4315_HT_AVAIL			21	/* 0x00200000 */

/* 4315 chip-specific ChipStatus register bits */
#define	CHIPC_CST4315_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R22_MASK
#define	CHIPC_CST4315_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R22_SHIFT
#define	CHIPC_CST4315_SDIO_MODE			0x00000004	/* gpio [8], sdio/usb mode */
#define	CHIPC_CST4315_RCAL_VALID		0x00000008
#define	CHIPC_CST4315_RCAL_VALUE_MASK		0x000001f0
#define	CHIPC_CST4315_RCAL_VALUE_SHIFT		4
#define	CHIPC_CST4315_PALDO_EXTPNP		0x00000200	/* PALDO is configured with external PNP */
#define	CHIPC_CST4315_CBUCK_MODE_MASK		0x00000c00
#define	CHIPC_CST4315_CBUCK_MODE_BURST		0x00000400
#define	CHIPC_CST4315_CBUCK_MODE_LPBURST	0x00000c00

/* 4319 resources */
#define	CHIPC_RES4319_CBUCK_LPOM		1	/* 0x00000002 */
#define	CHIPC_RES4319_CBUCK_BURST		2	/* 0x00000004 */
#define	CHIPC_RES4319_CBUCK_PWM			3	/* 0x00000008 */
#define	CHIPC_RES4319_CLDO_PU			4	/* 0x00000010 */
#define	CHIPC_RES4319_PALDO_PU			5	/* 0x00000020 */
#define	CHIPC_RES4319_ILP_REQUEST		6	/* 0x00000040 */
#define	CHIPC_RES4319_LNLDO1_PU			9	/* 0x00000200 */
#define	CHIPC_RES4319_OTP_PU			10	/* 0x00000400 */
#define	CHIPC_RES4319_LNLDO2_PU			12	/* 0x00001000 */
#define	CHIPC_RES4319_XTAL_PU			13	/* 0x00002000 */
#define	CHIPC_RES4319_ALP_AVAIL			14	/* 0x00004000 */
#define	CHIPC_RES4319_RX_PWRSW_PU		15	/* 0x00008000 */
#define	CHIPC_RES4319_TX_PWRSW_PU		16	/* 0x00010000 */
#define	CHIPC_RES4319_RFPLL_PWRSW_PU		17	/* 0x00020000 */
#define	CHIPC_RES4319_LOGEN_PWRSW_PU		18	/* 0x00040000 */
#define	CHIPC_RES4319_AFE_PWRSW_PU		19	/* 0x00080000 */
#define	CHIPC_RES4319_BBPLL_PWRSW_PU		20	/* 0x00100000 */
#define	CHIPC_RES4319_HT_AVAIL			21	/* 0x00200000 */

/* 4319 chip-specific ChipStatus register bits */
#define	CHIPC_CST4319_SPI_CPULESSUSB		0x00000001
#define	CHIPC_CST4319_SPI_CLK_POL		0x00000002
#define	CHIPC_CST4319_SPI_CLK_PH		0x00000008
#define	CHIPC_CST4319_SPROM_OTP_SEL_MASK	CHIPC_CST_SPROM_OTP_SEL_R23_MASK	/* gpio [7:6], SDIO CIS selection */
#define	CHIPC_CST4319_SPROM_OTP_SEL_SHIFT	CHIPC_CST_SPROM_OTP_SEL_R23_SHIFT
#define	CHIPC_CST4319_SDIO_USB_MODE		0x00000100	/* gpio [8], sdio/usb mode */
#define	CHIPC_CST4319_REMAP_SEL_MASK		0x00000600
#define	CHIPC_CST4319_ILPDIV_EN			0x00000800
#define	CHIPC_CST4319_XTAL_PD_POL		0x00001000
#define	CHIPC_CST4319_LPO_SEL			0x00002000
#define	CHIPC_CST4319_RES_INIT_MODE		0x0000c000
#define	CHIPC_CST4319_PALDO_EXTPNP		0x00010000	/* PALDO is configured with external PNP */
#define	CHIPC_CST4319_CBUCK_MODE_MASK		0x00060000
#define	CHIPC_CST4319_CBUCK_MODE_BURST		0x00020000
#define	CHIPC_CST4319_CBUCK_MODE_LPBURST	0x00060000
#define	CHIPC_CST4319_RCAL_VALID		0x01000000
#define	CHIPC_CST4319_RCAL_VALUE_MASK		0x3e000000
#define	CHIPC_CST4319_RCAL_VALUE_SHIFT		25

#define	CHIPC_PMU1_PLL0_CHIPCTL0		0
#define	CHIPC_PMU1_PLL0_CHIPCTL1		1
#define	CHIPC_PMU1_PLL0_CHIPCTL2		2
#define	CHIPC_CCTL_4319USB_XTAL_SEL_MASK	0x00180000
#define	CHIPC_CCTL_4319USB_XTAL_SEL_SHIFT	19
#define	CHIPC_CCTL_4319USB_48MHZ_PLL_SEL	1
#define	CHIPC_CCTL_4319USB_24MHZ_PLL_SEL	2

/* PMU resources for 4336 */
#define	CHIPC_RES4336_CBUCK_LPOM		0
#define	CHIPC_RES4336_CBUCK_BURST		1
#define	CHIPC_RES4336_CBUCK_LP_PWM		2
#define	CHIPC_RES4336_CBUCK_PWM			3
#define	CHIPC_RES4336_CLDO_PU			4
#define	CHIPC_RES4336_DIS_INT_RESET_PD		5
#define	CHIPC_RES4336_ILP_REQUEST		6
#define	CHIPC_RES4336_LNLDO_PU			7
#define	CHIPC_RES4336_LDO3P3_PU			8
#define	CHIPC_RES4336_OTP_PU			9
#define	CHIPC_RES4336_XTAL_PU			10
#define	CHIPC_RES4336_ALP_AVAIL			11
#define	CHIPC_RES4336_RADIO_PU			12
#define	CHIPC_RES4336_BG_PU			13
#define	CHIPC_RES4336_VREG1p4_PU_PU		14
#define	CHIPC_RES4336_AFE_PWRSW_PU		15
#define	CHIPC_RES4336_RX_PWRSW_PU		16
#define	CHIPC_RES4336_TX_PWRSW_PU		17
#define	CHIPC_RES4336_BB_PWRSW_PU		18
#define	CHIPC_RES4336_SYNTH_PWRSW_PU		19
#define	CHIPC_RES4336_MISC_PWRSW_PU		20
#define	CHIPC_RES4336_LOGEN_PWRSW_PU		21
#define	CHIPC_RES4336_BBPLL_PWRSW_PU		22
#define	CHIPC_RES4336_MACPHY_CLKAVAIL		23
#define	CHIPC_RES4336_HT_AVAIL			24
#define	CHIPC_RES4336_RSVD			25

/* 4336 chip-specific ChipStatus register bits */
#define	CHIPC_CST4336_SPI_MODE_MASK		0x00000001
#define	CHIPC_CST4336_SPROM_PRESENT		0x00000002
#define	CHIPC_CST4336_OTP_PRESENT		0x00000004
#define	CHIPC_CST4336_ARMREMAP_0		0x00000008
#define	CHIPC_CST4336_ILPDIV_EN_MASK		0x00000010
#define	CHIPC_CST4336_ILPDIV_EN_SHIFT		4
#define	CHIPC_CST4336_XTAL_PD_POL_MASK		0x00000020
#define	CHIPC_CST4336_XTAL_PD_POL_SHIFT		5
#define	CHIPC_CST4336_LPO_SEL_MASK		0x00000040
#define	CHIPC_CST4336_LPO_SEL_SHIFT		6
#define	CHIPC_CST4336_RES_INIT_MODE_MASK	0x00000180
#define	CHIPC_CST4336_RES_INIT_MODE_SHIFT	7
#define	CHIPC_CST4336_CBUCK_MODE_MASK		0x00000600
#define	CHIPC_CST4336_CBUCK_MODE_SHIFT		9

/* 4330 resources */
#define	CHIPC_RES4330_CBUCK_LPOM		0
#define	CHIPC_RES4330_CBUCK_BURST		1
#define	CHIPC_RES4330_CBUCK_LP_PWM		2
#define	CHIPC_RES4330_CBUCK_PWM			3
#define	CHIPC_RES4330_CLDO_PU			4
#define	CHIPC_RES4330_DIS_INT_RESET_PD		5
#define	CHIPC_RES4330_ILP_REQUEST		6
#define	CHIPC_RES4330_LNLDO_PU			7
#define	CHIPC_RES4330_LDO3P3_PU			8
#define	CHIPC_RES4330_OTP_PU			9
#define	CHIPC_RES4330_XTAL_PU			10
#define	CHIPC_RES4330_ALP_AVAIL			11
#define	CHIPC_RES4330_RADIO_PU			12
#define	CHIPC_RES4330_BG_PU			13
#define	CHIPC_RES4330_VREG1p4_PU_PU		14
#define	CHIPC_RES4330_AFE_PWRSW_PU		15
#define	CHIPC_RES4330_RX_PWRSW_PU		16
#define	CHIPC_RES4330_TX_PWRSW_PU		17
#define	CHIPC_RES4330_BB_PWRSW_PU		18
#define	CHIPC_RES4330_SYNTH_PWRSW_PU		19
#define	CHIPC_RES4330_MISC_PWRSW_PU		20
#define	CHIPC_RES4330_LOGEN_PWRSW_PU		21
#define	CHIPC_RES4330_BBPLL_PWRSW_PU		22
#define	CHIPC_RES4330_MACPHY_CLKAVAIL		23
#define	CHIPC_RES4330_HT_AVAIL			24
#define	CHIPC_RES4330_5gRX_PWRSW_PU		25
#define	CHIPC_RES4330_5gTX_PWRSW_PU		26
#define	CHIPC_RES4330_5g_LOGEN_PWRSW_PU	27

/* 4330 chip-specific ChipStatus register bits */
#define	CHIPC_CST4330_CHIPMODE_SDIOD(cs)	(((cs) & 0x7) < 6)	/* SDIO || gSPI */
#define	CHIPC_CST4330_CHIPMODE_USB20D(cs)	(((cs) & 0x7) >= 6)	/* USB || USBDA */
#define	CHIPC_CST4330_CHIPMODE_SDIO(cs)		(((cs) & 0x4) == 0)	/* SDIO */
#define	CHIPC_CST4330_CHIPMODE_GSPI(cs)		(((cs) & 0x6) == 4)	/* gSPI */
#define	CHIPC_CST4330_CHIPMODE_USB(cs)		(((cs) & 0x7) == 6)	/* USB packet-oriented */
#define	CHIPC_CST4330_CHIPMODE_USBDA(cs)	(((cs) & 0x7) == 7)	/* USB Direct Access */
#define	CHIPC_CST4330_OTP_PRESENT		0x00000010
#define	CHIPC_CST4330_LPO_AUTODET_EN		0x00000020
#define	CHIPC_CST4330_ARMREMAP_0		0x00000040
#define	CHIPC_CST4330_SPROM_PRESENT		0x00000080	/* takes priority over OTP if both set */
#define	CHIPC_CST4330_ILPDIV_EN			0x00000100
#define	CHIPC_CST4330_LPO_SEL			0x00000200
#define	CHIPC_CST4330_RES_INIT_MODE_SHIFT	10
#define	CHIPC_CST4330_RES_INIT_MODE_MASK	0x00000c00
#define	CHIPC_CST4330_CBUCK_MODE_SHIFT		12
#define	CHIPC_CST4330_CBUCK_MODE_MASK		0x00003000
#define	CHIPC_CST4330_CBUCK_POWER_OK		0x00004000
#define	CHIPC_CST4330_BB_PLL_LOCKED		0x00008000
#define	CHIPC_SOCDEVRAM_4330_BP_ADDR		0x1E000000
#define	CHIPC_SOCDEVRAM_4330_ARM_ADDR		0x00800000

/* 4313 resources */
#define	CHIPC_RES4313_BB_PU_RSRC		0
#define	CHIPC_RES4313_ILP_REQ_RSRC		1
#define	CHIPC_RES4313_XTAL_PU_RSRC		2
#define	CHIPC_RES4313_ALP_AVAIL_RSRC		3
#define	CHIPC_RES4313_RADIO_PU_RSRC		4
#define	CHIPC_RES4313_BG_PU_RSRC		5
#define	CHIPC_RES4313_VREG1P4_PU_RSRC		6
#define	CHIPC_RES4313_AFE_PWRSW_RSRC		7
#define	CHIPC_RES4313_RX_PWRSW_RSRC		8
#define	CHIPC_RES4313_TX_PWRSW_RSRC		9
#define	CHIPC_RES4313_BB_PWRSW_RSRC		10
#define	CHIPC_RES4313_SYNTH_PWRSW_RSRC		11
#define	CHIPC_RES4313_MISC_PWRSW_RSRC		12
#define	CHIPC_RES4313_BB_PLL_PWRSW_RSRC		13
#define	CHIPC_RES4313_HT_AVAIL_RSRC		14
#define	CHIPC_RES4313_MACPHY_CLK_AVAIL_RSRC	15

/* 4313 chip-specific ChipStatus register bits */
#define	CHIPC_CST4313_SPROM_PRESENT		1
#define	CHIPC_CST4313_OTP_PRESENT		2
#define	CHIPC_CST4313_SPROM_OTP_SEL_MASK	0x00000002
#define	CHIPC_CST4313_SPROM_OTP_SEL_SHIFT	0

/* 4313 Chip specific ChipControl register bits */
#define	CHIPC_CCTRL_4313_12MA_LED_DRIVE		0x00000007	/* 12 mA drive strengh for later 4313 */

/* 43228 resources */
#define	CHIPC_RES43228_NOT_USED			0
#define	CHIPC_RES43228_ILP_REQUEST		1
#define	CHIPC_RES43228_XTAL_PU			2
#define	CHIPC_RES43228_ALP_AVAIL		3
#define	CHIPC_RES43228_PLL_EN			4
#define	CHIPC_RES43228_HT_PHY_AVAIL		5

/* 43228 chipstatus  reg bits */
#define	CHIPC_CST43228_ILP_DIV_EN		0x1
#define	CHIPC_CST43228_OTP_PRESENT		0x2
#define	CHIPC_CST43228_SERDES_REFCLK_PADSEL	0x4
#define	CHIPC_CST43228_SDIO_MODE		0x8

#define	CHIPC_CST43228_SDIO_OTP_PRESENT		0x10
#define	CHIPC_CST43228_SDIO_RESET		0x20

/*
* Maximum delay for the PMU state transition in us.
* This is an upper bound intended for spinwaits etc.
*/
#define	CHIPC_PMU_MAX_TRANSITION_DLY		15000

/* PMU resource up transition time in ILP cycles */
#define	CHIPC_PMURES_UP_TRANSITION		2

/*
* Register eci_inputlo bitfield values.
* - BT packet type information bits [7:0]
*/
/*  [3:0] - Task (link) type */
#define	CHIPC_BT_ACL				0x00
#define	CHIPC_BT_SCO				0x01
#define	CHIPC_BT_eSCO				0x02
#define	CHIPC_BT_A2DP				0x03
#define	CHIPC_BT_SNIFF				0x04
#define	CHIPC_BT_PAGE_SCAN			0x05
#define	CHIPC_BT_INQUIRY_SCAN			0x06
#define	CHIPC_BT_PAGE				0x07
#define	CHIPC_BT_INQUIRY			0x08
#define	CHIPC_BT_MSS				0x09
#define	CHIPC_BT_PARK				0x0a
#define	CHIPC_BT_RSSISCAN			0x0b
#define	CHIPC_BT_MD_ACL				0x0c
#define	CHIPC_BT_MD_eSCO			0x0d
#define	CHIPC_BT_SCAN_WITH_SCO_LINK		0x0e
#define	CHIPC_BT_SCAN_WITHOUT_SCO_LINK		0x0f
/* [7:4] = packet duration code */
/* [8] - Master / Slave */
#define	CHIPC_BT_MASTER				0
#define	CHIPC_BT_SLAVE				1
/* [11:9] - multi-level priority */
#define	CHIPC_BT_LOWEST_PRIO			0x0
#define	CHIPC_BT_HIGHEST_PRIO			0x3

#endif /* _BHND_CORES_CHIPC_CHIPCREG_H_ */
