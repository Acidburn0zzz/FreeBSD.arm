/*-
 * Copyright (c) 2004 Texas A&M University
 * All rights reserved.
 *
 * Developer: Wm. Daryl Hawkins
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
 */

/*
 * Intel ICH Watchdog Timer (WDT) driver
 *
 * Originally developed by Wm. Daryl Hawkins of Texas A&M
 * Heavily modified by <des@FreeBSD.org>
 *
 * This is a tricky one.  The ICH WDT can't be treated as a regular PCI
 * device as it's actually an integrated function of the ICH LPC interface
 * bridge.  Detection is also awkward, because we can only infer the
 * presence of the watchdog timer from the fact that the machine has an
 * ICH chipset, or, on ACPI 2.x systems, by the presence of the 'WDDT'
 * ACPI table (although this driver does not support the ACPI detection
 * method).
 *
 * There is one slight problem on non-ACPI or ACPI 1.x systems: we have no
 * way of knowing if the WDT is permanently disabled (either by the BIOS
 * or in hardware).
 *
 * The WDT is programmed through I/O registers in the ACPI I/O space.
 * Intel swears it's always at offset 0x60, so we use that.
 *
 * For details about the ICH WDT, see Intel Application Note AP-725
 * (document no. 292273-001).  The WDT is also described in the individual
 * chipset datasheets, e.g. Intel82801EB ICH5 / 82801ER ICH5R Datasheet
 * (document no. 252516-001) sections 9.10 and 9.11.
 *
 * ICH6/7/8 support by Takeharu KATO <takeharu1219@ybb.ne.jp>
 * SoC PMC support by Denir Li <denir.li@cas-well.com>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/ichwd/ichwd.c 297255 2016-03-24 23:24:00Z mav $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/watchdog.h>

#include <isa/isavar.h>
#include <dev/pci/pcivar.h>

#include <dev/ichwd/ichwd.h>

static struct ichwd_device ichwd_devices[] = {
	{ DEVICEID_82801AA,  "Intel 82801AA watchdog timer",	1, 1 },
	{ DEVICEID_82801AB,  "Intel 82801AB watchdog timer",	1, 1 },
	{ DEVICEID_82801BA,  "Intel 82801BA watchdog timer",	2, 1 },
	{ DEVICEID_82801BAM, "Intel 82801BAM watchdog timer",	2, 1 },
	{ DEVICEID_82801CA,  "Intel 82801CA watchdog timer",	3, 1 },
	{ DEVICEID_82801CAM, "Intel 82801CAM watchdog timer",	3, 1 },
	{ DEVICEID_82801DB,  "Intel 82801DB watchdog timer",	4, 1 },
	{ DEVICEID_82801DBM, "Intel 82801DBM watchdog timer",	4, 1 },
	{ DEVICEID_82801E,   "Intel 82801E watchdog timer",	5, 1 },
	{ DEVICEID_82801EB,  "Intel 82801EB watchdog timer",	5, 1 },
	{ DEVICEID_82801EBR, "Intel 82801EB/ER watchdog timer",	5, 1 },
	{ DEVICEID_6300ESB,  "Intel 6300ESB watchdog timer",	5, 1 },
	{ DEVICEID_82801FBR, "Intel 82801FB/FR watchdog timer",	6, 2 },
	{ DEVICEID_ICH6M,    "Intel ICH6M watchdog timer",	6, 2 },
	{ DEVICEID_ICH6W,    "Intel ICH6W watchdog timer",	6, 2 },
	{ DEVICEID_ICH7,     "Intel ICH7 watchdog timer",	7, 2 },
	{ DEVICEID_ICH7DH,   "Intel ICH7DH watchdog timer",	7, 2 },
	{ DEVICEID_ICH7M,    "Intel ICH7M watchdog timer",	7, 2 },
	{ DEVICEID_ICH7MDH,  "Intel ICH7MDH watchdog timer",	7, 2 },
	{ DEVICEID_NM10,     "Intel NM10 watchdog timer",	7, 2 },
	{ DEVICEID_ICH8,     "Intel ICH8 watchdog timer",	8, 2 },
	{ DEVICEID_ICH8DH,   "Intel ICH8DH watchdog timer",	8, 2 },
	{ DEVICEID_ICH8DO,   "Intel ICH8DO watchdog timer",	8, 2 },
	{ DEVICEID_ICH8M,    "Intel ICH8M watchdog timer",	8, 2 },
	{ DEVICEID_ICH8ME,   "Intel ICH8M-E watchdog timer",	8, 2 },
	{ DEVICEID_63XXESB,  "Intel 63XXESB watchdog timer",	8, 2 },
	{ DEVICEID_ICH9,     "Intel ICH9 watchdog timer",	9, 2 },
	{ DEVICEID_ICH9DH,   "Intel ICH9DH watchdog timer",	9, 2 },
	{ DEVICEID_ICH9DO,   "Intel ICH9DO watchdog timer",	9, 2 },
	{ DEVICEID_ICH9M,    "Intel ICH9M watchdog timer",	9, 2 },
	{ DEVICEID_ICH9ME,   "Intel ICH9M-E watchdog timer",	9, 2 },
	{ DEVICEID_ICH9R,    "Intel ICH9R watchdog timer",	9, 2 },
	{ DEVICEID_ICH10,    "Intel ICH10 watchdog timer",	10, 2 },
	{ DEVICEID_ICH10D,   "Intel ICH10D watchdog timer",	10, 2 },
	{ DEVICEID_ICH10DO,  "Intel ICH10DO watchdog timer",	10, 2 },
	{ DEVICEID_ICH10R,   "Intel ICH10R watchdog timer",	10, 2 },
	{ DEVICEID_PCH,      "Intel PCH watchdog timer",	10, 2 },
	{ DEVICEID_PCHM,     "Intel PCH watchdog timer",	10, 2 },
	{ DEVICEID_P55,      "Intel P55 watchdog timer",	10, 2 },
	{ DEVICEID_PM55,     "Intel PM55 watchdog timer",	10, 2 },
	{ DEVICEID_H55,      "Intel H55 watchdog timer",	10, 2 },
	{ DEVICEID_QM57,     "Intel QM57 watchdog timer",       10, 2 },
	{ DEVICEID_H57,      "Intel H57 watchdog timer",        10, 2 },
	{ DEVICEID_HM55,     "Intel HM55 watchdog timer",       10, 2 },
	{ DEVICEID_Q57,      "Intel Q57 watchdog timer",        10, 2 },
	{ DEVICEID_HM57,     "Intel HM57 watchdog timer",       10, 2 },
	{ DEVICEID_PCHMSFF,  "Intel PCHMSFF watchdog timer",    10, 2 },
	{ DEVICEID_QS57,     "Intel QS57 watchdog timer",       10, 2 },
	{ DEVICEID_3400,     "Intel 3400 watchdog timer",       10, 2 },
	{ DEVICEID_3420,     "Intel 3420 watchdog timer",       10, 2 },
	{ DEVICEID_3450,     "Intel 3450 watchdog timer",       10, 2 },
	{ DEVICEID_CPT0,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT1,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT2,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT3,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT4,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT5,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT6,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT7,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT8,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT9,     "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT10,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT11,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT12,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT13,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT14,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT15,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT16,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT17,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT18,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT19,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT20,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT21,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT22,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT23,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT24,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT25,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT26,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT27,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT28,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT29,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT30,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_CPT31,    "Intel Cougar Point watchdog timer",	10, 2 },
	{ DEVICEID_PATSBURG_LPC1, "Intel Patsburg watchdog timer",	10, 2 },
	{ DEVICEID_PATSBURG_LPC2, "Intel Patsburg watchdog timer",	10, 2 },
	{ DEVICEID_PPT0,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT1,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT2,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT3,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT4,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT5,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT6,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT7,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT8,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT9,     "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT10,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT11,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT12,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT13,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT14,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT15,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT16,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT17,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT18,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT19,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT20,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT21,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT22,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT23,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT24,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT25,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT26,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT27,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT28,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT29,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT30,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_PPT31,    "Intel Panther Point watchdog timer",	10, 2 },
	{ DEVICEID_LPT0,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT1,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT2,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT3,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT4,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT5,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT6,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT7,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT8,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT9,     "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT10,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT11,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT12,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT13,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT14,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT15,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT16,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT17,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT18,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT19,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT20,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT21,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT22,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT23,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT24,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT25,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT26,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT27,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT28,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT29,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT30,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_LPT31,    "Intel Lynx Point watchdog timer",		10, 2 },
	{ DEVICEID_WCPT1,    "Intel Wildcat Point watchdog timer",	10, 2 },
	{ DEVICEID_WCPT2,    "Intel Wildcat Point watchdog timer",	10, 2 },
	{ DEVICEID_WCPT3,    "Intel Wildcat Point watchdog timer",	10, 2 },
	{ DEVICEID_WCPT4,    "Intel Wildcat Point watchdog timer",	10, 2 },
	{ DEVICEID_WCPT6,    "Intel Wildcat Point watchdog timer",	10, 2 },
	{ DEVICEID_WBG0,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG1,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG2,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG3,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG4,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG5,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG6,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG7,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG8,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG9,     "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG10,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG11,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG12,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG13,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG14,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG15,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG16,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG17,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG18,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG19,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG20,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG21,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG22,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG23,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG24,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG25,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG26,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG27,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG28,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG29,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG30,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_WBG31,    "Intel Wellsburg watchdog timer",		10, 2 },
	{ DEVICEID_LPT_LP0,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP1,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP2,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP3,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP4,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP5,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP6,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_LPT_LP7,  "Intel Lynx Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP1, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP2, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP3, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP5, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP6, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP7, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_WCPT_LP9, "Intel Wildcat Point-LP watchdog timer",	10, 2 },
	{ DEVICEID_DH89XXCC_LPC,  "Intel DH89xxCC watchdog timer",	10, 2 },
	{ DEVICEID_COLETOCRK_LPC, "Intel Coleto Creek watchdog timer",  10, 2 },
	{ DEVICEID_AVN0,     "Intel Avoton/Rangeley SoC watchdog timer",10, 3 },
	{ DEVICEID_AVN1,     "Intel Avoton/Rangeley SoC watchdog timer",10, 3 },
	{ DEVICEID_AVN2,     "Intel Avoton/Rangeley SoC watchdog timer",10, 3 },
	{ DEVICEID_AVN3,     "Intel Avoton/Rangeley SoC watchdog timer",10, 3 },
	{ DEVICEID_BAYTRAIL, "Intel Bay Trail SoC watchdog timer",	10, 3 },
	{ DEVICEID_BRASWELL, "Intel Braswell SoC watchdog timer",	10, 3 },
	{ 0, NULL, 0, 0 },
};

static devclass_t ichwd_devclass;

#define ichwd_read_tco_1(sc, off) \
	bus_read_1((sc)->tco_res, (off))
#define ichwd_read_tco_2(sc, off) \
	bus_read_2((sc)->tco_res, (off))
#define ichwd_read_tco_4(sc, off) \
	bus_read_4((sc)->tco_res, (off))
#define ichwd_read_smi_4(sc, off) \
	bus_read_4((sc)->smi_res, (off))
#define ichwd_read_gcs_4(sc, off) \
	bus_read_4((sc)->gcs_res, (off))
/* NB: TCO version 3 devices use the gcs_res resource for the PMC register. */
#define ichwd_read_pmc_4(sc, off) \
	bus_read_4((sc)->gcs_res, (off))

#define ichwd_write_tco_1(sc, off, val) \
	bus_write_1((sc)->tco_res, (off), (val))
#define ichwd_write_tco_2(sc, off, val) \
	bus_write_2((sc)->tco_res, (off), (val))
#define ichwd_write_tco_4(sc, off, val) \
	bus_write_4((sc)->tco_res, (off), (val))
#define ichwd_write_smi_4(sc, off, val) \
	bus_write_4((sc)->smi_res, (off), (val))
#define ichwd_write_gcs_4(sc, off, val) \
	bus_write_4((sc)->gcs_res, (off), (val))
/* NB: TCO version 3 devices use the gcs_res resource for the PMC register. */
#define ichwd_write_pmc_4(sc, off, val) \
	bus_write_4((sc)->gcs_res, (off), (val))

#define ichwd_verbose_printf(dev, ...) \
	do {						\
		if (bootverbose)			\
			device_printf(dev, __VA_ARGS__);\
	} while (0)

/*
 * Disable the watchdog timeout SMI handler.
 *
 * Apparently, some BIOSes install handlers that reset or disable the
 * watchdog timer instead of resetting the system, so we disable the SMI
 * (by clearing the SMI_TCO_EN bit of the SMI_EN register) to prevent this
 * from happening.
 */
static __inline void
ichwd_smi_disable(struct ichwd_softc *sc)
{
	ichwd_write_smi_4(sc, SMI_EN, ichwd_read_smi_4(sc, SMI_EN) & ~SMI_TCO_EN);
}

/*
 * Enable the watchdog timeout SMI handler.  See above for details.
 */
static __inline void
ichwd_smi_enable(struct ichwd_softc *sc)
{
	ichwd_write_smi_4(sc, SMI_EN, ichwd_read_smi_4(sc, SMI_EN) | SMI_TCO_EN);
}

/*
 * Check if the watchdog SMI triggering is enabled.
 */
static __inline int
ichwd_smi_is_enabled(struct ichwd_softc *sc)
{
	return ((ichwd_read_smi_4(sc, SMI_EN) & SMI_TCO_EN) != 0);
}

/*
 * Reset the watchdog status bits.
 */
static __inline void
ichwd_sts_reset(struct ichwd_softc *sc)
{
	/*
	 * The watchdog status bits are set to 1 by the hardware to
	 * indicate various conditions.  They can be cleared by software
	 * by writing a 1, not a 0.
	 */
	ichwd_write_tco_2(sc, TCO1_STS, TCO_TIMEOUT);
	/*
	 * According to Intel's docs, clearing SECOND_TO_STS and BOOT_STS must
	 * be done in two separate operations.
	 */
	ichwd_write_tco_2(sc, TCO2_STS, TCO_SECOND_TO_STS);
	ichwd_write_tco_2(sc, TCO2_STS, TCO_BOOT_STS);
}

/*
 * Enable the watchdog timer by clearing the TCO_TMR_HALT bit in the
 * TCO1_CNT register.  This is complicated by the need to preserve bit 9
 * of that same register, and the requirement that all other bits must be
 * written back as zero.
 */
static __inline void
ichwd_tmr_enable(struct ichwd_softc *sc)
{
	uint16_t cnt;

	cnt = ichwd_read_tco_2(sc, TCO1_CNT) & TCO_CNT_PRESERVE;
	ichwd_write_tco_2(sc, TCO1_CNT, cnt & ~TCO_TMR_HALT);
	sc->active = 1;
	ichwd_verbose_printf(sc->device, "timer enabled\n");
}

/*
 * Disable the watchdog timer.  See above for details.
 */
static __inline void
ichwd_tmr_disable(struct ichwd_softc *sc)
{
	uint16_t cnt;

	cnt = ichwd_read_tco_2(sc, TCO1_CNT) & TCO_CNT_PRESERVE;
	ichwd_write_tco_2(sc, TCO1_CNT, cnt | TCO_TMR_HALT);
	sc->active = 0;
	ichwd_verbose_printf(sc->device, "timer disabled\n");
}

/*
 * Reload the watchdog timer: writing anything to any of the lower five
 * bits of the TCO_RLD register reloads the timer from the last value
 * written to TCO_TMR.
 */
static __inline void
ichwd_tmr_reload(struct ichwd_softc *sc)
{
	if (sc->tco_version == 1)
		ichwd_write_tco_1(sc, TCO_RLD, 1);
	else
		ichwd_write_tco_2(sc, TCO_RLD, 1);
}

/*
 * Set the initial timeout value.  Note that this must always be followed
 * by a reload.
 */
static __inline void
ichwd_tmr_set(struct ichwd_softc *sc, unsigned int timeout)
{

	if (timeout < TCO_RLD_TMR_MIN)
		timeout = TCO_RLD_TMR_MIN;

	if (sc->tco_version == 1) {
		uint8_t tmr_val8 = ichwd_read_tco_1(sc, TCO_TMR1);

		tmr_val8 &= (~TCO_RLD1_TMR_MAX & 0xff);
		if (timeout > TCO_RLD1_TMR_MAX)
			timeout = TCO_RLD1_TMR_MAX;
		tmr_val8 |= timeout;
		ichwd_write_tco_1(sc, TCO_TMR1, tmr_val8);
	} else {
		uint16_t tmr_val16 = ichwd_read_tco_2(sc, TCO_TMR2);

		tmr_val16 &= (~TCO_RLD2_TMR_MAX & 0xffff);
		if (timeout > TCO_RLD2_TMR_MAX)
			timeout = TCO_RLD2_TMR_MAX;
		tmr_val16 |= timeout;
		ichwd_write_tco_2(sc, TCO_TMR2, tmr_val16);
	}

	sc->timeout = timeout;

	ichwd_verbose_printf(sc->device, "timeout set to %u ticks\n", timeout);
}

static __inline int
ichwd_clear_noreboot(struct ichwd_softc *sc)
{
	uint32_t status;
	int rc = 0;

	/* try to clear the NO_REBOOT bit */
	switch (sc->tco_version) {
	case 1:
		status = pci_read_config(sc->ich, ICH_GEN_STA, 1);
		status &= ~ICH_GEN_STA_NO_REBOOT;
		pci_write_config(sc->ich, ICH_GEN_STA, status, 1);
		status = pci_read_config(sc->ich, ICH_GEN_STA, 1);
		if (status & ICH_GEN_STA_NO_REBOOT)
			rc = EIO;
		break;
	case 2:
		status = ichwd_read_gcs_4(sc, 0);
		status &= ~ICH_GCS_NO_REBOOT;
		ichwd_write_gcs_4(sc, 0, status);
		status = ichwd_read_gcs_4(sc, 0);
		if (status & ICH_GCS_NO_REBOOT)
			rc = EIO;
		break;
	case 3:
		status = ichwd_read_pmc_4(sc, 0);
		status &= ~ICH_PMC_NO_REBOOT;
		ichwd_write_pmc_4(sc, 0, status);
		status = ichwd_read_pmc_4(sc, 0);
		if (status & ICH_PMC_NO_REBOOT)
			rc = EIO;
		break;
	default:
		ichwd_verbose_printf(sc->device,
		    "Unknown TCO Version: %d, can't set NO_REBOOT.\n",
		    sc->tco_version);
		break;
	}

	if (rc)
		device_printf(sc->device,
		    "ICH WDT present but disabled in BIOS or hardware\n");

	return (rc);
}

/*
 * Watchdog event handler - called by the framework to enable or disable
 * the watchdog or change the initial timeout value.
 */
static void
ichwd_event(void *arg, unsigned int cmd, int *error)
{
	struct ichwd_softc *sc = arg;
	unsigned int timeout;

	/* convert from power-of-two-ns to WDT ticks */
	cmd &= WD_INTERVAL;
	timeout = ((uint64_t)1 << cmd) / ICHWD_TICK;
	if (cmd) {
		if (!sc->active)
			ichwd_tmr_enable(sc);
		if (timeout != sc->timeout)
			ichwd_tmr_set(sc, timeout);
		ichwd_tmr_reload(sc);
		*error = 0;
	} else {
		if (sc->active)
			ichwd_tmr_disable(sc);
	}
}

static device_t
ichwd_find_ich_lpc_bridge(struct ichwd_device **id_p)
{
	struct ichwd_device *id;
	device_t ich = NULL;

	/* look for an ICH LPC interface bridge */
	for (id = ichwd_devices; id->desc != NULL; ++id)
		if ((ich = pci_find_device(VENDORID_INTEL, id->device)) != NULL)
			break;

	if (ich == NULL)
		return (NULL);

	if (id_p)
		*id_p = id;

	return (ich);
}

/*
 * Look for an ICH LPC interface bridge.  If one is found, register an
 * ichwd device.  There can be only one.
 */
static void
ichwd_identify(driver_t *driver, device_t parent)
{
	struct ichwd_device *id_p;
	device_t ich = NULL;
	device_t dev;
	uint32_t base_address;
	int rc;

	ich = ichwd_find_ich_lpc_bridge(&id_p);
	if (ich == NULL)
		return;

	/* good, add child to bus */
	if ((dev = device_find_child(parent, driver->name, 0)) == NULL)
		dev = BUS_ADD_CHILD(parent, 0, driver->name, 0);

	if (dev == NULL)
		return;

	switch (id_p->tco_version) {
	case 1:
		break;
	case 2:
		/* get RCBA (root complex base address) */
		base_address = pci_read_config(ich, ICH_RCBA, 4);
		rc = bus_set_resource(ich, SYS_RES_MEMORY, 0,
		    (base_address & 0xffffc000) + ICH_GCS_OFFSET,
		    ICH_GCS_SIZE);
		if (rc)
			ichwd_verbose_printf(dev,
			    "Can not set TCO v%d memory resource for RCBA\n",
			    id_p->tco_version);
		break;
	case 3:
		/* get PBASE (Power Management Controller base address) */
		base_address = pci_read_config(ich, ICH_PBASE, 4);
		rc = bus_set_resource(ich, SYS_RES_MEMORY, 0,
		    (base_address & 0xfffffe00) + ICH_PMC_OFFSET,
		    ICH_PMC_SIZE);
		if (rc)
			ichwd_verbose_printf(dev,
			    "Can not set TCO v%d memory resource for PBASE\n",
			    id_p->tco_version);
		break;
	default:
		ichwd_verbose_printf(dev,
		    "Can not set unknown TCO v%d memory resource for unknown base address\n",
		    id_p->tco_version);
		break;
	}
}

static int
ichwd_probe(device_t dev)
{
	struct ichwd_device *id_p;

	/* Do not claim some ISA PnP device by accident. */
	if (isa_get_logicalid(dev) != 0)
		return (ENXIO);

	if (ichwd_find_ich_lpc_bridge(&id_p) == NULL)
		return (ENXIO);

	device_set_desc_copy(dev, id_p->desc);
	return (0);
}

static int
ichwd_attach(device_t dev)
{
	struct ichwd_softc *sc;
	struct ichwd_device *id_p;
	device_t ich;
	unsigned int pmbase = 0;

	sc = device_get_softc(dev);
	sc->device = dev;

	ich = ichwd_find_ich_lpc_bridge(&id_p);
	if (ich == NULL) {
		device_printf(sc->device, "Can not find ICH device.\n");
		goto fail;
	}
	sc->ich = ich;
	sc->ich_version = id_p->ich_version;
	sc->tco_version = id_p->tco_version;

	/* get ACPI base address */
	pmbase = pci_read_config(ich, ICH_PMBASE, 2) & ICH_PMBASE_MASK;
	if (pmbase == 0) {
		device_printf(dev, "ICH PMBASE register is empty\n");
		goto fail;
	}

	/* allocate I/O register space */
	sc->smi_rid = 0;
	sc->smi_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->smi_rid,
	    pmbase + SMI_BASE, pmbase + SMI_BASE + SMI_LEN - 1, SMI_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->smi_res == NULL) {
		device_printf(dev, "unable to reserve SMI registers\n");
		goto fail;
	}

	sc->tco_rid = 1;
	sc->tco_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->tco_rid,
	    pmbase + TCO_BASE, pmbase + TCO_BASE + TCO_LEN - 1, TCO_LEN,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->tco_res == NULL) {
		device_printf(dev, "unable to reserve TCO registers\n");
		goto fail;
	}

	sc->gcs_rid = 0;
	if (sc->tco_version >= 2) {
		sc->gcs_res = bus_alloc_resource_any(ich, SYS_RES_MEMORY,
		    &sc->gcs_rid, RF_ACTIVE|RF_SHAREABLE);
		if (sc->gcs_res == NULL) {
			device_printf(dev, "unable to reserve GCS registers\n");
			goto fail;
		}
	}

	if (ichwd_clear_noreboot(sc) != 0)
		goto fail;

	/*
	 * Determine if we are coming up after a watchdog-induced reset.  Some
	 * BIOSes may clear this bit at bootup, preventing us from reporting
	 * this case on such systems.  We clear this bit in ichwd_sts_reset().
	 */
	if ((ichwd_read_tco_2(sc, TCO2_STS) & TCO_SECOND_TO_STS) != 0)
		device_printf(dev,
		    "resuming after hardware watchdog timeout\n");

	/* reset the watchdog status registers */
	ichwd_sts_reset(sc);

	/* make sure the WDT starts out inactive */
	ichwd_tmr_disable(sc);

	/* register the watchdog event handler */
	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, ichwd_event, sc, 0);

	/* disable the SMI handler */
	sc->smi_enabled = ichwd_smi_is_enabled(sc);
	ichwd_smi_disable(sc);

	return (0);
 fail:
	sc = device_get_softc(dev);
	if (sc->tco_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->tco_rid, sc->tco_res);
	if (sc->smi_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->smi_rid, sc->smi_res);
	if (sc->gcs_res != NULL)
		bus_release_resource(ich, SYS_RES_MEMORY,
		    sc->gcs_rid, sc->gcs_res);

	return (ENXIO);
}

static int
ichwd_detach(device_t dev)
{
	struct ichwd_softc *sc;
	device_t ich = NULL;

	sc = device_get_softc(dev);

	/* halt the watchdog timer */
	if (sc->active)
		ichwd_tmr_disable(sc);

	/* enable the SMI handler */
	if (sc->smi_enabled != 0)
		ichwd_smi_enable(sc);

	/* deregister event handler */
	if (sc->ev_tag != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);
	sc->ev_tag = NULL;

	/* reset the watchdog status registers */
	ichwd_sts_reset(sc);

	/* deallocate I/O register space */
	bus_release_resource(dev, SYS_RES_IOPORT, sc->tco_rid, sc->tco_res);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->smi_rid, sc->smi_res);

	/* deallocate memory resource */
	ich = ichwd_find_ich_lpc_bridge(NULL);
	if (sc->gcs_res && ich)
		bus_release_resource(ich, SYS_RES_MEMORY, sc->gcs_rid,
		    sc->gcs_res);

	return (0);
}

static device_method_t ichwd_methods[] = {
	DEVMETHOD(device_identify, ichwd_identify),
	DEVMETHOD(device_probe,	ichwd_probe),
	DEVMETHOD(device_attach, ichwd_attach),
	DEVMETHOD(device_detach, ichwd_detach),
	DEVMETHOD(device_shutdown, ichwd_detach),
	{0,0}
};

static driver_t ichwd_driver = {
	"ichwd",
	ichwd_methods,
	sizeof(struct ichwd_softc),
};

DRIVER_MODULE(ichwd, isa, ichwd_driver, ichwd_devclass, NULL, NULL);
