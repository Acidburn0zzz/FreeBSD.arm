/*-
 * Copyright (c) 2016 Michal Meloun <mmel@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/nvidia/tegra_pcie.c 298627 2016-04-26 11:53:37Z br $");

/*
 * Nvidia Integrated PCI/PCI-Express controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/devmap.h>

#include <machine/intr.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include <machine/resource.h>
#include <machine/bus.h>

#include "ofw_bus_if.h"
#include "pcib_if.h"

#include <arm/nvidia/tegra_pmc.h>

/* --- Move to ofw_pci.c/.h ----------------------- */

struct tegra_pci_range {
	/* parsed phys.hi */
	int 		nonrelocatable;
	int		prefetchable;
	int		aliased;
	int		space_code;	/* In native format (not shifted)*/
	int		bus;
	int		device;
	int		function;
	int		reg;
	pci_addr_t	pci_addr;	/* PCI Address */
	bus_addr_t	host_addr;	/* Host bus address*/
	bus_size_t	size;		/* Range size */
};

static int
tegra_pci_get_ranges(phandle_t node,  struct tegra_pci_range **ranges)
{
	int host_address_cells, pci_address_cells, size_cells;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int nranges;
	int i, j, k;
	uint32_t flags;
	uint64_t tmp;

	host_address_cells = 1;
	pci_address_cells = 3;
	size_cells = 2;
	OF_getencprop(OF_parent(node), "#address-cells", &host_address_cells,
	    sizeof(host_address_cells));
	OF_getencprop(node, "#address-cells", &pci_address_cells,
	    sizeof(pci_address_cells));
	OF_getencprop(node, "#size-cells", &size_cells, sizeof(size_cells));

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges <= 0)
		return (-1);
	nranges = nbase_ranges / sizeof(cell_t) /
	    (pci_address_cells + host_address_cells + size_cells);

	*ranges = malloc(nranges * sizeof(struct tegra_pci_range),
	    M_DEVBUF, M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < nranges; i++) {
		flags =  base_ranges[j++];
		(*ranges)[i].nonrelocatable =
		   flags & OFW_PCI_PHYS_HI_NONRELOCATABLE ? 1 : 0;
		(*ranges)[i].prefetchable =
		   flags & OFW_PCI_PHYS_HI_PREFETCHABLE ? 1 : 0;
		(*ranges)[i].aliased =
		   flags & OFW_PCI_PHYS_HI_ALIASED ? 1 : 0;
		(*ranges)[i].space_code = flags & OFW_PCI_PHYS_HI_SPACEMASK;
		(*ranges)[i].bus = OFW_PCI_PHYS_HI_BUS(flags);
		(*ranges)[i].device = OFW_PCI_PHYS_HI_DEVICE(flags);
		(*ranges)[i].function = OFW_PCI_PHYS_HI_FUNCTION(flags);
		(*ranges)[i].reg = flags & OFW_PCI_PHYS_HI_REGISTERMASK;

		tmp = 0;
		for (k = 0; k < pci_address_cells - 1; k++) {
			tmp <<= 32;
			tmp |= base_ranges[j++];
		}
		(*ranges)[i].pci_addr = (pci_addr_t)tmp;

		tmp = 0;
		for (k = 0; k < host_address_cells; k++) {
			tmp <<= 32;
			tmp |= base_ranges[j++];
		}
		(*ranges)[i].host_addr = (bus_addr_t)tmp;
		tmp = 0;

		for (k = 0; k < size_cells; k++) {
			tmp <<= 32;
			tmp |= base_ranges[j++];
		}
		(*ranges)[i].size = (bus_size_t)tmp;
	}

	free(base_ranges, M_DEVBUF);
	return (nranges);
}

/* -------------------------------------------------------------------------- */
#define	AFI_AXI_BAR0_SZ				0x000
#define	AFI_AXI_BAR1_SZ				0x004
#define	AFI_AXI_BAR2_SZ				0x008
#define	AFI_AXI_BAR3_SZ				0x00c
#define	AFI_AXI_BAR4_SZ				0x010
#define	AFI_AXI_BAR5_SZ				0x014
#define	AFI_AXI_BAR0_START			0x018
#define	AFI_AXI_BAR1_START			0x01c
#define	AFI_AXI_BAR2_START			0x020
#define	AFI_AXI_BAR3_START			0x024
#define	AFI_AXI_BAR4_START			0x028
#define	AFI_AXI_BAR5_START			0x02c
#define	AFI_FPCI_BAR0				0x030
#define	AFI_FPCI_BAR1				0x034
#define	AFI_FPCI_BAR2				0x038
#define	AFI_FPCI_BAR3				0x03c
#define	AFI_FPCI_BAR4				0x040
#define	AFI_FPCI_BAR5				0x044
#define	AFI_MSI_BAR_SZ				0x060
#define	AFI_MSI_FPCI_BAR_ST			0x064
#define	AFI_MSI_AXI_BAR_ST			0x068


#define	AFI_AXI_BAR6_SZ				0x134
#define	AFI_AXI_BAR7_SZ				0x138
#define	AFI_AXI_BAR8_SZ				0x13c
#define	AFI_AXI_BAR6_START			0x140
#define	AFI_AXI_BAR7_START			0x144
#define	AFI_AXI_BAR8_START			0x148
#define	AFI_FPCI_BAR6				0x14c
#define	AFI_FPCI_BAR7				0x150
#define	AFI_FPCI_BAR8				0x154

#define	AFI_CONFIGURATION			0x0ac
#define	 AFI_CONFIGURATION_EN_FPCI			(1 << 0)

#define	AFI_FPCI_ERROR_MASKS			0x0b0
#define	AFI_INTR_MASK				0x0b4
#define	 AFI_INTR_MASK_MSI_MASK				(1 << 8)
#define	 AFI_INTR_MASK_INT_MASK				(1 << 0)

#define	AFI_INTR_CODE				0x0b8
#define	 AFI_INTR_CODE_MASK				0xf
#define	 AFI_INTR_CODE_INT_CODE_INI_SLVERR		1
#define	 AFI_INTR_CODE_INT_CODE_INI_DECERR		2
#define	 AFI_INTR_CODE_INT_CODE_TGT_SLVERR		3
#define	 AFI_INTR_CODE_INT_CODE_TGT_DECERR		4
#define	 AFI_INTR_CODE_INT_CODE_TGT_WRERR		5
#define	 AFI_INTR_CODE_INT_CODE_SM_MSG			6
#define	 AFI_INTR_CODE_INT_CODE_DFPCI_DECERR		7
#define	 AFI_INTR_CODE_INT_CODE_AXI_DECERR		8
#define	 AFI_INTR_CODE_INT_CODE_FPCI_TIMEOUT		9
#define	 AFI_INTR_CODE_INT_CODE_PE_PRSNT_SENSE		10
#define	 AFI_INTR_CODE_INT_CODE_PE_CLKREQ_SENSE		11
#define	 AFI_INTR_CODE_INT_CODE_CLKCLAMP_SENSE		12
#define	 AFI_INTR_CODE_INT_CODE_RDY4PD_SENSE		13
#define	 AFI_INTR_CODE_INT_CODE_P2P_ERROR		14


#define	AFI_INTR_SIGNATURE			0x0bc
#define	AFI_UPPER_FPCI_ADDRESS			0x0c0
#define	AFI_SM_INTR_ENABLE			0x0c4
#define	 AFI_SM_INTR_RP_DEASSERT			(1 << 14)
#define	 AFI_SM_INTR_RP_ASSERT				(1 << 13)
#define	 AFI_SM_INTR_HOTPLUG				(1 << 12)
#define	 AFI_SM_INTR_PME				(1 << 11)
#define	 AFI_SM_INTR_FATAL_ERROR			(1 << 10)
#define	 AFI_SM_INTR_UNCORR_ERROR			(1 <<  9)
#define	 AFI_SM_INTR_CORR_ERROR				(1 <<  8)
#define	 AFI_SM_INTR_INTD_DEASSERT			(1 <<  7)
#define	 AFI_SM_INTR_INTC_DEASSERT			(1 <<  6)
#define	 AFI_SM_INTR_INTB_DEASSERT			(1 <<  5)
#define	 AFI_SM_INTR_INTA_DEASSERT			(1 <<  4)
#define	 AFI_SM_INTR_INTD_ASSERT			(1 <<  3)
#define	 AFI_SM_INTR_INTC_ASSERT			(1 <<  2)
#define	 AFI_SM_INTR_INTB_ASSERT			(1 <<  1)
#define	 AFI_SM_INTR_INTA_ASSERT			(1 <<  0)

#define	AFI_AFI_INTR_ENABLE			0x0c8
#define	 AFI_AFI_INTR_ENABLE_CODE(code)			(1 << (code))

#define	AFI_PCIE_CONFIG				0x0f8
#define	 AFI_PCIE_CONFIG_PCIE_DISABLE(x)		(1 << ((x) + 1))
#define	 AFI_PCIE_CONFIG_PCIE_DISABLE_ALL		0x6
#define	 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK	(0xf << 20)
#define	 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_XBAR2_1	(0x0 << 20)
#define	 AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_XBAR4_1	(0x1 << 20)

#define	AFI_FUSE				0x104
#define	 AFI_FUSE_PCIE_T0_GEN2_DIS			(1 << 2)

#define	AFI_PEX0_CTRL				0x110
#define	AFI_PEX1_CTRL				0x118
#define	AFI_PEX2_CTRL				0x128
#define	 AFI_PEX_CTRL_OVERRIDE_EN			(1 << 4)
#define	 AFI_PEX_CTRL_REFCLK_EN				(1 << 3)
#define	 AFI_PEX_CTRL_CLKREQ_EN				(1 << 1)
#define	 AFI_PEX_CTRL_RST_L				(1 << 0)

#define	AFI_AXI_BAR6_SZ				0x134
#define	AFI_AXI_BAR7_SZ				0x138
#define	AFI_AXI_BAR8_SZ				0x13c
#define	AFI_AXI_BAR6_START			0x140
#define	AFI_AXI_BAR7_START			0x144
#define	AFI_AXI_BAR8_START			0x148
#define	AFI_FPCI_BAR6				0x14c
#define	AFI_FPCI_BAR7				0x150
#define	AFI_FPCI_BAR8				0x154
#define	AFI_PLLE_CONTROL			0x160
#define	 AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL	(1 << 9)
#define	 AFI_PLLE_CONTROL_BYPASS_PCIE2PLLE_CONTROL	(1 << 8)
#define	 AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN		(1 << 1)
#define	 AFI_PLLE_CONTROL_PCIE2PLLE_CONTROL_EN		(1 << 0)

#define	AFI_PEXBIAS_CTRL			0x168

/* FPCI Address space */
#define	FPCI_MAP_IO			0xfdfc000000ULL
#define	FPCI_MAP_TYPE0_CONFIG		0xfdfc000000ULL
#define	FPCI_MAP_TYPE1_CONFIG		0xfdff000000ULL
#define	FPCI_MAP_EXT_TYPE0_CONFIG	0xfe00000000ULL
#define	FPCI_MAP_EXT_TYPE1_CONFIG	0xfe10000000ULL

/* Configuration space */
#define	RP_VEND_XP	0x00000F00
#define	 RP_VEND_XP_DL_UP	(1 << 30)

#define	RP_PRIV_MISC	0x00000FE0
#define	 RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT (0xE << 0)
#define	 RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT (0xF << 0)

#define	RP_LINK_CONTROL_STATUS			0x00000090
#define	 RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE	0x20000000
#define	 RP_LINK_CONTROL_STATUS_LINKSTAT_MASK	0x3fff0000

#define	TEGRA_PCIE_LINKUP_TIMEOUT	200

#define	DEBUG
#ifdef DEBUG
#define	debugf(fmt, args...) do { printf(fmt,##args); } while (0)
#else
#define	debugf(fmt, args...)
#endif

/*
 * Configuration space format:
 *    [27:24] extended register
 *    [23:16] bus
 *    [15:11] slot (device)
 *    [10: 8] function
 *    [ 7: 0] register
 */
#define	PCI_CFG_EXT_REG(reg)	((((reg) >> 8) & 0x0f) << 24)
#define	PCI_CFG_BUS(bus)	(((bus) & 0xff) << 16)
#define	PCI_CFG_DEV(dev)	(((dev) & 0x1f) << 11)
#define	PCI_CFG_FUN(fun)	(((fun) & 0x07) << 8)
#define	PCI_CFG_BASE_REG(reg)	((reg)  & 0xff)

#define	PADS_WR4(_sc, _r, _v)	bus_write_4((_sc)-pads_mem_res, (_r), (_v))
#define	PADS_RD4(_sc, _r)	bus_read_4((_sc)->pads_mem_res, (_r))
#define	AFI_WR4(_sc, _r, _v)	bus_write_4((_sc)->afi_mem_res, (_r), (_v))
#define	AFI_RD4(_sc, _r)	bus_read_4((_sc)->afi_mem_res, (_r))

static struct {
	bus_size_t	axi_start;
	bus_size_t	fpci_start;
	bus_size_t	size;
} bars[] = {
    {AFI_AXI_BAR0_START, AFI_FPCI_BAR0, AFI_AXI_BAR0_SZ},	/* BAR 0 */
    {AFI_AXI_BAR1_START, AFI_FPCI_BAR1, AFI_AXI_BAR1_SZ},	/* BAR 1 */
    {AFI_AXI_BAR2_START, AFI_FPCI_BAR2, AFI_AXI_BAR2_SZ},	/* BAR 2 */
    {AFI_AXI_BAR3_START, AFI_FPCI_BAR3, AFI_AXI_BAR3_SZ},	/* BAR 3 */
    {AFI_AXI_BAR4_START, AFI_FPCI_BAR4, AFI_AXI_BAR4_SZ},	/* BAR 4 */
    {AFI_AXI_BAR5_START, AFI_FPCI_BAR5, AFI_AXI_BAR5_SZ},	/* BAR 5 */
    {AFI_AXI_BAR6_START, AFI_FPCI_BAR6, AFI_AXI_BAR6_SZ},	/* BAR 6 */
    {AFI_AXI_BAR7_START, AFI_FPCI_BAR7, AFI_AXI_BAR7_SZ},	/* BAR 7 */
    {AFI_AXI_BAR8_START, AFI_FPCI_BAR8, AFI_AXI_BAR8_SZ},	/* BAR 8 */
    {AFI_MSI_AXI_BAR_ST, AFI_MSI_FPCI_BAR_ST, AFI_MSI_BAR_SZ},	/* MSI 9 */
};

/* Compatible devices. */
static struct ofw_compat_data compat_data[] = {
	{"nvidia,tegra124-pcie",	1},
	{NULL,		 		0},
};

struct tegra_pcib_port {
	int		enabled;
	int 		port_idx;		/* chip port index */
	int		num_lanes;		/* number of lanes */
	bus_size_t	afi_pex_ctrl;		/* offset of afi_pex_ctrl */

	/* Config space properties. */
	bus_addr_t	rp_base_addr;		/* PA of config window */
	bus_size_t	rp_size;		/* size of config window */
	bus_space_handle_t cfg_handle;		/* handle of config window */
};

#define	TEGRA_PCIB_MAX_PORTS	3
struct tegra_pcib_softc {
	device_t		dev;
	struct mtx		mtx;
	struct ofw_bus_iinfo	pci_iinfo;
	struct rman		pref_mem_rman;
	struct rman		mem_rman;
	struct rman		io_rman;
	struct resource		*pads_mem_res;
	struct resource		*afi_mem_res;
	struct resource		*cfg_mem_res;
	struct resource 	*irq_res;
	struct resource 	*msi_irq_res;
	void			*intr_cookie;
	void			*msi_intr_cookie;

	struct tegra_pci_range	mem_range;
	struct tegra_pci_range	pref_mem_range;
	struct tegra_pci_range	io_range;

	phy_t			phy;
	clk_t			clk_pex;
	clk_t			clk_afi;
	clk_t			clk_pll_e;
	clk_t			clk_cml;
	hwreset_t			hwreset_pex;
	hwreset_t			hwreset_afi;
	hwreset_t			hwreset_pcie_x;
	regulator_t		supply_avddio_pex;
	regulator_t		supply_dvddio_pex;
	regulator_t		supply_avdd_pex_pll;
	regulator_t		supply_hvdd_pex;
	regulator_t		supply_hvdd_pex_pll_e;
	regulator_t		supply_vddio_pex_ctl;
	regulator_t		supply_avdd_pll_erefe;

	int			busnr;		/* host bridge bus number */
	uint32_t		msi_bitmap;
	bus_addr_t		cfg_base_addr;	/* base address of config */
	bus_size_t		cfg_cur_offs; 	/* currently mapped window */
	bus_space_handle_t 	cfg_handle;	/* handle of config window */
	bus_space_tag_t 	bus_tag;	/* tag of config window */
	int			lanes_cfg;
	int			num_ports;
	struct tegra_pcib_port *ports[TEGRA_PCIB_MAX_PORTS];
};

/* ------------------------------------------------------------------------- */
/*
 * Resource manager
 */
static int
tegra_pcib_rman_init(struct tegra_pcib_softc *sc)
{
	int err;
	char buf[64];

	/* Memory management. */
	sc->pref_mem_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s prefetchable memory space",
	    device_get_nameunit(sc->dev));
	sc->pref_mem_rman.rm_descr = strdup(buf, M_DEVBUF);
	err = rman_init(&sc->pref_mem_rman);
	if (err)
		return (err);

	sc->mem_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s non prefetchable memory space",
	    device_get_nameunit(sc->dev));
	sc->mem_rman.rm_descr = strdup(buf, M_DEVBUF);
	err = rman_init(&sc->mem_rman);
	if (err)
		return (err);

	sc->io_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s I/O space",
	    device_get_nameunit(sc->dev));
	sc->io_rman.rm_descr = strdup(buf, M_DEVBUF);
	err = rman_init(&sc->io_rman);
	if (err) {
		rman_fini(&sc->mem_rman);
		return (err);
	}

	err = rman_manage_region(&sc->pref_mem_rman,
	    sc->pref_mem_range.host_addr,
	    sc->pref_mem_range.host_addr + sc->pref_mem_range.size - 1);
	if (err)
		goto error;
	err = rman_manage_region(&sc->mem_rman,
	    sc->mem_range.host_addr,
	    sc->mem_range.host_addr + sc->mem_range.size - 1);
	if (err)
		goto error;
	err = rman_manage_region(&sc->io_rman,
	    sc->io_range.pci_addr,
	    sc->io_range.pci_addr + sc->io_range.size - 1);
	if (err)
		goto error;
	return (0);

error:
	rman_fini(&sc->pref_mem_rman);
	rman_fini(&sc->mem_rman);
	rman_fini(&sc->io_rman);
	return (err);
}

static struct rman *
tegra_pcib_rman(struct tegra_pcib_softc *sc, int type, u_int flags)
{

	switch (type) {
	case SYS_RES_IOPORT:
		return (&sc->io_rman);
	case SYS_RES_MEMORY:
		if (flags & RF_PREFETCHABLE)
			return (&sc->pref_mem_rman);
		else
			return (&sc->mem_rman);
	default:
		break;
	}

	return (NULL);
}

static struct resource *
tegra_pcib_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct tegra_pcib_softc *sc;
	struct rman *rm;
	struct resource *res;

	debugf("%s: enter %d start %#jx end %#jx count %#jx\n", __func__,
	    type, start, end, count);
	sc = device_get_softc(dev);

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type ==  PCI_RES_BUS) {
		  return (pci_domain_alloc_bus(0, child, rid, start, end, count,
					       flags));
	}
#endif

	rm = tegra_pcib_rman(sc, type, flags);

	if (rm == NULL) {
		res = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
		    type, rid, start, end, count, flags);

		return (res);
	}

	if (bootverbose) {
		device_printf(dev,
		    "rman_reserve_resource: start=%#jx, end=%#jx, count=%#jx\n",
		    start, end, count);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		goto fail;
	rman_set_rid(res, *rid);
	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, res)) {
			rman_release_resource(res);
			goto fail;
		}
	}
	return (res);

fail:
	if (bootverbose) {
		device_printf(dev, "%s FAIL: type=%d, rid=%d, "
		    "start=%016jx, end=%016jx, count=%016jx, flags=%x\n",
		    __func__, type, *rid, start, end, count, flags);
	}

	return (NULL);
}

static int
tegra_pcib_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	struct tegra_pcib_softc *sc;
	struct rman *rm;

	sc = device_get_softc(dev);
	debugf("%s: %d rid %x\n",  __func__, type, rid);

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type == PCI_RES_BUS)
		return (pci_domain_release_bus(0, child, rid, res));
#endif

	rm = tegra_pcib_rman(sc, type, rman_get_flags(res));
	if (rm != NULL) {
		KASSERT(rman_is_region_manager(res, rm), ("rman mismatch"));
		rman_release_resource(res);
	}

	return (bus_generic_release_resource(dev, child, type, rid, res));
}

static int
tegra_pcib_adjust_resource(device_t dev, device_t child, int type,
			    struct resource *res, rman_res_t start, rman_res_t end)
{
	struct tegra_pcib_softc *sc;
	struct rman *rm;

	sc = device_get_softc(dev);
	debugf("%s: %d start %jx end %jx \n", __func__, type, start, end);

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type == PCI_RES_BUS)
		return (pci_domain_adjust_bus(0, child, res, start, end));
#endif

	rm = tegra_pcib_rman(sc, type, rman_get_flags(res));
	if (rm != NULL)
		return (rman_adjust_resource(res, start, end));
	return (bus_generic_adjust_resource(dev, child, type, res, start, end));
}
extern bus_space_tag_t fdtbus_bs_tag;
static int
tegra_pcib_pcie_activate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct tegra_pcib_softc *sc;
	vm_offset_t start;
	void *p;
	int rv;

	sc = device_get_softc(dev);
	rv = rman_activate_resource(r);
	if (rv != 0)
		return (rv);
	switch(type) {
	case SYS_RES_IOPORT:
		start = rman_get_start(r) + sc->io_range.host_addr;
		break;
	default:
		start = rman_get_start(r);
		rman_get_start(r);
		break;
	}

	if (bootverbose)
		printf("%s: start %zx, len %jd\n", __func__, start,
			rman_get_size(r));

	p = pmap_mapdev(start, (vm_size_t)rman_get_size(r));
	rman_set_virtual(r, p);
	rman_set_bustag(r, fdtbus_bs_tag);
	rman_set_bushandle(r, (u_long)p);
	return (0);
}

/* ------------------------------------------------------------------------- */
/*
 * IVARs
 */
static int
tegra_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct tegra_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->busnr;
		return (0);
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	}

	return (ENOENT);
}

static int
tegra_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct tegra_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->busnr = value;
		return (0);
	}

	return (ENOENT);
}

static int
tegra_pcib_maxslots(device_t dev)
{
	return (16);
}


static int
tegra_pcib_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct tegra_pcib_softc *sc;

	sc = device_get_softc(bus);
	device_printf(bus, "route pin %d for device %d.%d to %ju\n",
		      pin, pci_get_slot(dev), pci_get_function(dev),
		      rman_get_start(sc->irq_res));

	return (rman_get_start(sc->irq_res));
}

static int
tegra_pcbib_map_cfg(struct tegra_pcib_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg)
{
	bus_size_t offs;
	int rv;

	offs = sc->cfg_base_addr;
	offs |= PCI_CFG_BUS(bus) | PCI_CFG_DEV(slot) | PCI_CFG_FUN(func) |
	    PCI_CFG_EXT_REG(reg);
	if ((sc->cfg_handle != 0) && (sc->cfg_cur_offs == offs))
		return (0);
	if (sc->cfg_handle != 0)
		bus_space_unmap(sc->bus_tag, sc->cfg_handle, 0x800);

	rv = bus_space_map(sc->bus_tag, offs, 0x800, 0, &sc->cfg_handle);
	if (rv != 0)
		device_printf(sc->dev, "Cannot map config space\n");
	else
		sc->cfg_cur_offs = offs;
	return (rv);
}

static uint32_t
tegra_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct tegra_pcib_softc *sc;
	bus_space_handle_t hndl;
	uint32_t off;
	uint32_t val;
	int rv, i;

	sc = device_get_softc(dev);
	if (bus == 0) {
		if (func != 0)
			return (0xFFFFFFFF);
		for (i = 0; i < TEGRA_PCIB_MAX_PORTS; i++) {
			if ((sc->ports[i] != NULL) &&
			    (sc->ports[i]->port_idx == slot)) {
				hndl = sc->ports[i]->cfg_handle;
				off = reg & 0xFFF;
				break;
			}
		}
		if (i >= TEGRA_PCIB_MAX_PORTS)
			return (0xFFFFFFFF);
	} else {
		rv = tegra_pcbib_map_cfg(sc, bus, slot, func, reg);
		if (rv != 0)
			return (0xFFFFFFFF);
		hndl = sc->cfg_handle;
		off = PCI_CFG_BASE_REG(reg);
	}

	val = bus_space_read_4(sc->bus_tag, hndl, off & ~3);
	switch (bytes) {
	case 4:
		break;
	case 2:
		if (off & 3)
			val >>= 16;
		val &= 0xffff;
		break;
	case 1:
		val >>= ((off & 3) << 3);
		val &= 0xff;
		break;
	}
	return val;
}

static void
tegra_pcib_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct tegra_pcib_softc *sc;
	bus_space_handle_t hndl;
	uint32_t off;
	uint32_t val2;
	int rv, i;

	sc = device_get_softc(dev);
	if (bus == 0) {
		if (func != 0)
			return;
		for (i = 0; i < TEGRA_PCIB_MAX_PORTS; i++) {
			if ((sc->ports[i] != NULL) &&
			    (sc->ports[i]->port_idx == slot)) {
				hndl = sc->ports[i]->cfg_handle;
				off = reg & 0xFFF;
				break;
			}
		}
		if (i >= TEGRA_PCIB_MAX_PORTS)
			return;
	} else {
		rv = tegra_pcbib_map_cfg(sc, bus, slot, func, reg);
		if (rv != 0)
			return;
		hndl = sc->cfg_handle;
		off = PCI_CFG_BASE_REG(reg);
	}

	switch (bytes) {
	case 4:
		bus_space_write_4(sc->bus_tag, hndl, off, val);
		break;
	case 2:
		val2 = bus_space_read_4(sc->bus_tag, hndl, off & ~3);
		val2 &= ~(0xffff << ((off & 3) << 3));
		val2 |= ((val & 0xffff) << ((off & 3) << 3));
		bus_space_write_4(sc->bus_tag, hndl, off & ~3, val2);
		break;
	case 1:
		val2 = bus_space_read_4(sc->bus_tag, hndl, off & ~3);
		val2 &= ~(0xff << ((off & 3) << 3));
		val2 |= ((val & 0xff) << ((off & 3) << 3));
		bus_space_write_4(sc->bus_tag, hndl, off & ~3, val2);
		break;
	}
}

static int tegra_pci_intr(void *arg)
{
	struct tegra_pcib_softc *sc = arg;
	uint32_t code, signature;

	code = bus_read_4(sc->afi_mem_res, AFI_INTR_CODE) & AFI_INTR_CODE_MASK;
	signature = bus_read_4(sc->afi_mem_res, AFI_INTR_SIGNATURE);
	bus_write_4(sc->afi_mem_res, AFI_INTR_CODE, 0);
	if (code == AFI_INTR_CODE_INT_CODE_SM_MSG)
		return(FILTER_STRAY);

	printf("tegra_pci_intr: code %x sig %x\n", code, signature);
	return (FILTER_HANDLED);
}

#if defined(TEGRA_PCI_MSI)
static int
tegra_pcib_map_msi(device_t dev, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	struct tegra_pcib_softc *sc;

	sc = device_get_softc(dev);
	irq = irq - MSI_IRQ;

	/* validate parameters */
	if (isclr(&sc->msi_bitmap, irq)) {
		device_printf(dev, "invalid MSI 0x%x\n", irq);
		return (EINVAL);
	}

	tegra_msi_data(irq, addr, data);

	debugf("%s: irq: %d addr: %jx data: %x\n",
	    __func__, irq, *addr, *data);

	return (0);
}

static int
tegra_pcib_alloc_msi(device_t dev, device_t child, int count,
    int maxcount __unused, int *irqs)
{
	struct tegra_pcib_softc *sc;
	u_int start = 0, i;

	if (powerof2(count) == 0 || count > MSI_IRQ_NUM)
		return (EINVAL);

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);

	for (start = 0; (start + count) < MSI_IRQ_NUM; start++) {
		for (i = start; i < start + count; i++) {
			if (isset(&sc->msi_bitmap, i))
				break;
		}
		if (i == start + count)
			break;
	}

	if ((start + count) == MSI_IRQ_NUM) {
		mtx_unlock(&sc->mtx);
		return (ENXIO);
	}

	for (i = start; i < start + count; i++) {
		setbit(&sc->msi_bitmap, i);
		irqs[i] = MSI_IRQ + i;
	}
	debugf("%s: start: %x count: %x\n", __func__, start, count);

	mtx_unlock(&sc->mtx);
	return (0);
}

static int
tegra_pcib_release_msi(device_t dev, device_t child, int count, int *irqs)
{
	struct tegra_pcib_softc *sc;
	u_int i;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);

	for (i = 0; i < count; i++)
		clrbit(&sc->msi_bitmap, irqs[i] - MSI_IRQ);

	mtx_unlock(&sc->mtx);
	return (0);
}
#endif

static bus_size_t
tegra_pcib_pex_ctrl(struct tegra_pcib_softc *sc, int port)
{
	if (port >= TEGRA_PCIB_MAX_PORTS)
		panic("invalid port number: %d\n", port);

	if (port == 0)
		return (AFI_PEX0_CTRL);
	else if (port == 1)
		return (AFI_PEX1_CTRL);
	else if (port == 2)
		return (AFI_PEX2_CTRL);
	else
		panic("invalid port number: %d\n", port);
}

static int
tegra_pcib_enable_fdt_resources(struct tegra_pcib_softc *sc)
{
	int rv;

	rv = hwreset_assert(sc->hwreset_pcie_x);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'pcie_x' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_afi);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert  'afi' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_pex);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert  'pex' reset\n");
		return (rv);
	}

	tegra_powergate_power_off(TEGRA_POWERGATE_PCX);

	/* Power supplies. */
	rv = regulator_enable(sc->supply_avddio_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'avddio_pex' regulator\n");
		return (rv);
	}
	rv = regulator_enable(sc->supply_dvddio_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'dvddio_pex' regulator\n");
		return (rv);
	}
	rv = regulator_enable(sc->supply_avdd_pex_pll);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'avdd-pex-pll' regulator\n");
		return (rv);
	}

	rv = regulator_enable(sc->supply_hvdd_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'hvdd-pex-supply' regulator\n");
		return (rv);
	}
	rv = regulator_enable(sc->supply_hvdd_pex_pll_e);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'hvdd-pex-pll-e-supply' regulator\n");
		return (rv);
	}
	rv = regulator_enable(sc->supply_vddio_pex_ctl);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'vddio-pex-ctl' regulator\n");
		return (rv);
	}
	rv = regulator_enable(sc->supply_avdd_pll_erefe);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot enable 'avdd-pll-erefe-supply' regulator\n");
		return (rv);
	}

	rv = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_PCX,
	    sc->clk_pex, sc->hwreset_pex);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'PCX' powergate\n");
		return (rv);
	}

	rv = hwreset_deassert(sc->hwreset_afi);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot unreset 'afi' reset\n");
		return (rv);
	}

	rv = clk_enable(sc->clk_afi);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'afi' clock\n");
		return (rv);
	}

	rv = clk_enable(sc->clk_cml);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'cml' clock\n");
		return (rv);
	}

	rv = clk_enable(sc->clk_pll_e);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'pll_e' clock\n");
		return (rv);
	}
	return (0);
}

static struct tegra_pcib_port *
tegra_pcib_parse_port(struct tegra_pcib_softc *sc, phandle_t node)
{
	struct tegra_pcib_port *port;
	uint32_t tmp[5];
	char tmpstr[6];
	int rv;

	port = malloc(sizeof(struct tegra_pcib_port), M_DEVBUF, M_WAITOK);

	rv = OF_getprop(node, "status", tmpstr, sizeof(tmpstr));
	if (rv <= 0 || strcmp(tmpstr, "okay") == 0 ||
	   strcmp(tmpstr, "ok") == 0)
		port->enabled = 1;
	else
		port->enabled = 0;

	rv = OF_getencprop(node, "assigned-addresses", tmp, sizeof(tmp));
	if (rv != sizeof(tmp)) {
		device_printf(sc->dev, "Cannot parse assigned-address: %d\n",
		    rv);
		goto fail;
	}
	port->rp_base_addr = tmp[2];
	port->rp_size = tmp[4];
	port->port_idx = OFW_PCI_PHYS_HI_DEVICE(tmp[0]) - 1;
	if (port->port_idx >= TEGRA_PCIB_MAX_PORTS) {
		device_printf(sc->dev, "Invalid port index: %d\n",
		    port->port_idx);
		goto fail;
	}
	/* XXX - TODO:
	 * Implement proper function for parsing pci "reg" property:
	 *  - it have PCI bus format
	 *  - its relative to matching "assigned-addresses"
	 */
	rv = OF_getencprop(node, "reg", tmp, sizeof(tmp));
	if (rv != sizeof(tmp)) {
		device_printf(sc->dev, "Cannot parse reg: %d\n", rv);
		goto fail;
	}
	port->rp_base_addr += tmp[2];

	rv = OF_getencprop(node, "nvidia,num-lanes", &port->num_lanes,
	    sizeof(port->num_lanes));
	if (rv != sizeof(port->num_lanes)) {
		device_printf(sc->dev, "Cannot parse nvidia,num-lanes: %d\n",
		    rv);
		goto fail;
	}
	if (port->num_lanes > 4) {
		device_printf(sc->dev, "Invalid nvidia,num-lanes: %d\n",
		    port->num_lanes);
		goto fail;
	}

	port->afi_pex_ctrl = tegra_pcib_pex_ctrl(sc, port->port_idx);
	sc->lanes_cfg |= port->num_lanes << (4 * port->port_idx);

	return (port);
fail:
	free(port, M_DEVBUF);
	return (NULL);
}


static int
tegra_pcib_parse_fdt_resources(struct tegra_pcib_softc *sc, phandle_t node)
{
	phandle_t child;
	struct tegra_pcib_port *port;
	int rv;

	/* Power supplies. */
	rv = regulator_get_by_ofw_property(sc->dev, "avddio-pex-supply",
	    &sc->supply_avddio_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'avddio-pex' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "dvddio-pex-supply",
	     &sc->supply_dvddio_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'dvddio-pex' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "avdd-pex-pll-supply",
	     &sc->supply_avdd_pex_pll);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'avdd-pex-pll' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "hvdd-pex-supply",
	     &sc->supply_hvdd_pex);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'hvdd-pex' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "hvdd-pex-pll-e-supply",
	     &sc->supply_hvdd_pex_pll_e);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'hvdd-pex-pll-e' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "vddio-pex-ctl-supply",
	    &sc->supply_vddio_pex_ctl);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'vddio-pex-ctl' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, "avdd-pll-erefe-supply",
	     &sc->supply_avdd_pll_erefe);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get 'avdd-pll-erefe' regulator\n");
		return (ENXIO);
	}

	/* Resets. */
	rv = hwreset_get_by_ofw_name(sc->dev, "pex", &sc->hwreset_pex);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pex' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, "afi", &sc->hwreset_afi);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'afi' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, "pcie_x", &sc->hwreset_pcie_x);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pcie_x' reset\n");
		return (ENXIO);
	}

	/* Clocks. */
	rv = clk_get_by_ofw_name(sc->dev, "pex", &sc->clk_pex);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pex' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, "afi", &sc->clk_afi);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'afi' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, "pll_e", &sc->clk_pll_e);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pll_e' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, "cml", &sc->clk_cml);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'cml' clock\n");
		return (ENXIO);
	}

	/* Phy. */
	rv = phy_get_by_ofw_name(sc->dev, "pcie", &sc->phy);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pcie' phy\n");
		return (ENXIO);
	}

	/* Ports */
	sc->num_ports = 0;
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		port = tegra_pcib_parse_port(sc, child);
		if (port == NULL) {
			device_printf(sc->dev, "Cannot parse PCIe port node\n");
			return (ENXIO);
		}
		sc->ports[sc->num_ports++] = port;
	}

	return (0);
}

static int
tegra_pcib_decode_ranges(struct tegra_pcib_softc *sc,
    struct tegra_pci_range *ranges, int nranges)
{
	int i;

	for (i = 2; i < nranges; i++) {
		if (ranges[i].space_code == OFW_PCI_PHYS_HI_SPACE_IO) {
			if (sc->io_range.size != 0) {
				device_printf(sc->dev,
				    "Duplicated IO range found in DT\n");
				return (ENXIO);
			}
			sc->io_range = ranges[i];
		}
		if ((ranges[i].space_code == OFW_PCI_PHYS_HI_SPACE_MEM32) &&
		    !ranges[i].prefetchable) {
			if (sc->mem_range.size != 0) {
				device_printf(sc->dev,
				    "Duplicated memory range found in DT\n");
				return (ENXIO);
			}
			sc->mem_range = ranges[i];
		}
		if ((ranges[i].space_code == OFW_PCI_PHYS_HI_SPACE_MEM32) &&
		    ranges[i].prefetchable) {
			if (sc->pref_mem_range.size != 0) {
				device_printf(sc->dev,
				    "Duplicated memory range found in DT\n");
				return (ENXIO);
			}
			sc->pref_mem_range = ranges[i];
		}
	}
	if ((sc->io_range.size == 0) || (sc->mem_range.size == 0)
	    || (sc->pref_mem_range.size == 0)) {
		device_printf(sc->dev,
		    " Not all required ranges are found in DT\n");
		return (ENXIO);
	}
	return (0);
}

/*
 * Hardware config.
 */
static int
tegra_pcib_wait_for_link(struct tegra_pcib_softc *sc,
    struct tegra_pcib_port *port)
{
	uint32_t reg;
	int i;


	/* Setup link detection. */
	reg = tegra_pcib_read_config(sc->dev, 0, port->port_idx, 0,
	    RP_PRIV_MISC, 4);
	reg &= ~RP_PRIV_MISC_PRSNT_MAP_EP_ABSNT;
	reg |= RP_PRIV_MISC_PRSNT_MAP_EP_PRSNT;
	tegra_pcib_write_config(sc->dev, 0, port->port_idx, 0,
	    RP_PRIV_MISC, reg, 4);

	for (i = TEGRA_PCIE_LINKUP_TIMEOUT; i > 0; i--) {
		reg = tegra_pcib_read_config(sc->dev, 0, port->port_idx, 0,
		    RP_VEND_XP, 4);
		if (reg & RP_VEND_XP_DL_UP)
				break;

	}
	if (i <= 0)
		return (ETIMEDOUT);

	for (i = TEGRA_PCIE_LINKUP_TIMEOUT; i > 0; i--) {
		reg = tegra_pcib_read_config(sc->dev, 0, port->port_idx, 0,
		    RP_LINK_CONTROL_STATUS, 4);
		if (reg & RP_LINK_CONTROL_STATUS_DL_LINK_ACTIVE)
				break;

	}
	if (i <= 0)
		return (ETIMEDOUT);
	return (0);
}

static void
tegra_pcib_port_enable(struct tegra_pcib_softc *sc, int port_num)
{
	struct tegra_pcib_port *port;
	uint32_t reg;
	int rv;

	port = sc->ports[port_num];

	/* Put port to reset. */
	reg = AFI_RD4(sc, port->afi_pex_ctrl);
	reg &= ~AFI_PEX_CTRL_RST_L;
	AFI_WR4(sc, port->afi_pex_ctrl, reg);
	AFI_RD4(sc, port->afi_pex_ctrl);
	DELAY(10);

	/* Enable clocks. */
	reg |= AFI_PEX_CTRL_REFCLK_EN;
	reg |= AFI_PEX_CTRL_CLKREQ_EN;
	reg |= AFI_PEX_CTRL_OVERRIDE_EN;
	AFI_WR4(sc, port->afi_pex_ctrl, reg);
	AFI_RD4(sc, port->afi_pex_ctrl);
	DELAY(100);

	/* Release reset. */
	reg |= AFI_PEX_CTRL_RST_L;
	AFI_WR4(sc, port->afi_pex_ctrl, reg);

	rv = tegra_pcib_wait_for_link(sc, port);
	if (bootverbose)
		device_printf(sc->dev, " port %d (%d lane%s): Link is %s\n",
			 port->port_idx, port->num_lanes,
			 port->num_lanes > 1 ? "s": "",
			 rv == 0 ? "up": "down");
}


static void
tegra_pcib_port_disable(struct tegra_pcib_softc *sc, uint32_t port_num)
{
	struct tegra_pcib_port *port;
	uint32_t reg;

	port = sc->ports[port_num];

	/* Put port to reset. */
	reg = AFI_RD4(sc, port->afi_pex_ctrl);
	reg &= ~AFI_PEX_CTRL_RST_L;
	AFI_WR4(sc, port->afi_pex_ctrl, reg);
	AFI_RD4(sc, port->afi_pex_ctrl);
	DELAY(10);

	/* Disable clocks. */
	reg &= ~AFI_PEX_CTRL_CLKREQ_EN;
	reg &= ~AFI_PEX_CTRL_REFCLK_EN;
	AFI_WR4(sc, port->afi_pex_ctrl, reg);

	if (bootverbose)
		device_printf(sc->dev, " port %d (%d lane%s): Disabled\n",
			 port->port_idx, port->num_lanes,
			 port->num_lanes > 1 ? "s": "");
}

static void
tegra_pcib_set_bar(struct tegra_pcib_softc *sc, int bar, uint32_t axi,
    uint64_t fpci, uint32_t size, int is_memory)
{
	uint32_t fpci_reg;
	uint32_t axi_reg;
	uint32_t size_reg;

	axi_reg = axi & ~0xFFF;
	size_reg = size >> 12;
	fpci_reg = (uint32_t)(fpci >> 8) & ~0xF;
	fpci_reg |= is_memory ? 0x1 : 0x0;
	AFI_WR4(sc, bars[bar].axi_start, axi_reg);
	AFI_WR4(sc, bars[bar].size, size_reg);
	AFI_WR4(sc, bars[bar].fpci_start, fpci_reg);
}

static int
tegra_pcib_enable(struct tegra_pcib_softc *sc, uint32_t port)
{
	int rv;
	int i;
	uint32_t reg;

	rv = tegra_pcib_enable_fdt_resources(sc);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable FDT resources\n");
		return (rv);
	}
	/* Enable PLLE control. */
	reg = AFI_RD4(sc, AFI_PLLE_CONTROL);
	reg &= ~AFI_PLLE_CONTROL_BYPASS_PADS2PLLE_CONTROL;
	reg |= AFI_PLLE_CONTROL_PADS2PLLE_CONTROL_EN;
	AFI_WR4(sc, AFI_PLLE_CONTROL, reg);

	/* Set bias pad. */
	AFI_WR4(sc, AFI_PEXBIAS_CTRL, 0);

	/* Configure mode and ports. */
	reg = AFI_RD4(sc, AFI_PCIE_CONFIG);
	reg &= ~AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_MASK;
	if (sc->lanes_cfg == 0x14) {
		if (bootverbose)
			device_printf(sc->dev,
			    "Using x1,x4 configuration\n");
		reg |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_XBAR4_1;
	} else if (sc->lanes_cfg == 0x12) {
		if (bootverbose)
			device_printf(sc->dev,
			    "Using x1,x2 configuration\n");
		reg |= AFI_PCIE_CONFIG_SM2TMS0_XBAR_CONFIG_XBAR2_1;
	} else {
		device_printf(sc->dev,
		    "Unsupported lanes configuration: 0x%X\n", sc->lanes_cfg);
	}
	reg |= AFI_PCIE_CONFIG_PCIE_DISABLE_ALL;
	for (i = 0; i < TEGRA_PCIB_MAX_PORTS; i++) {
		if ((sc->ports[i] != NULL))
			reg &=
			 ~AFI_PCIE_CONFIG_PCIE_DISABLE(sc->ports[i]->port_idx);
	}
	AFI_WR4(sc, AFI_PCIE_CONFIG, reg);

	/* Enable Gen2 support. */
	reg = AFI_RD4(sc, AFI_FUSE);
	reg &= ~AFI_FUSE_PCIE_T0_GEN2_DIS;
	AFI_WR4(sc, AFI_FUSE, reg);

	/* Enable PCIe phy. */
	rv = phy_enable(sc->dev, sc->phy);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable phy\n");
		return (rv);
	}

	rv = hwreset_deassert(sc->hwreset_pcie_x);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot unreset  'pci_x' reset\n");
		return (rv);
	}

	/* Enable config space. */
	reg = AFI_RD4(sc, AFI_CONFIGURATION);
	reg |= AFI_CONFIGURATION_EN_FPCI;
	AFI_WR4(sc, AFI_CONFIGURATION, reg);

	/* Enable AFI errors. */
	reg = 0;
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_INI_SLVERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_INI_DECERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_TGT_SLVERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_TGT_DECERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_TGT_WRERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_SM_MSG);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_DFPCI_DECERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_AXI_DECERR);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_FPCI_TIMEOUT);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_PE_PRSNT_SENSE);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_PE_CLKREQ_SENSE);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_CLKCLAMP_SENSE);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_RDY4PD_SENSE);
	reg |= AFI_AFI_INTR_ENABLE_CODE(AFI_INTR_CODE_INT_CODE_P2P_ERROR);
	AFI_WR4(sc, AFI_AFI_INTR_ENABLE, reg);
	AFI_WR4(sc, AFI_SM_INTR_ENABLE, 0xffffffff);

	/* Enable INT, disable MSI. */
	AFI_WR4(sc, AFI_INTR_MASK, AFI_INTR_MASK_INT_MASK);

	/* Mask all FPCI errors. */
	AFI_WR4(sc, AFI_FPCI_ERROR_MASKS, 0);

	/* Setup AFI translation windows. */
	/* BAR 0 - type 1 extended configuration. */
	tegra_pcib_set_bar(sc, 0, rman_get_start(sc->cfg_mem_res),
	   FPCI_MAP_EXT_TYPE1_CONFIG, rman_get_size(sc->cfg_mem_res), 0);

	/* BAR 1 - downstream I/O. */
	tegra_pcib_set_bar(sc, 1, sc->io_range.host_addr, FPCI_MAP_IO,
	    sc->io_range.size, 0);

	/* BAR 2 - downstream prefetchable memory 1:1. */
	tegra_pcib_set_bar(sc, 2, sc->pref_mem_range.host_addr,
	    sc->pref_mem_range.host_addr, sc->pref_mem_range.size, 1);

	/* BAR 3 - downstream not prefetchable memory 1:1 .*/
	tegra_pcib_set_bar(sc, 3, sc->mem_range.host_addr,
	    sc->mem_range.host_addr, sc->mem_range.size, 1);

	/* BAR 3-8 clear. */
	tegra_pcib_set_bar(sc, 4, 0, 0, 0, 0);
	tegra_pcib_set_bar(sc, 5, 0, 0, 0, 0);
	tegra_pcib_set_bar(sc, 6, 0, 0, 0, 0);
	tegra_pcib_set_bar(sc, 7, 0, 0, 0, 0);
	tegra_pcib_set_bar(sc, 8, 0, 0, 0, 0);

	/* MSI BAR - clear. */
	tegra_pcib_set_bar(sc, 9, 0, 0, 0, 0);
	return(0);
}

static int
tegra_pcib_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "Nvidia Integrated PCI/PCI-E Controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
tegra_pcib_attach(device_t dev)
{
	struct tegra_pcib_softc *sc;
	phandle_t node;
	uint32_t unit;
	int rv;
	int rid;
	int nranges;
	struct tegra_pci_range *ranges;
	struct tegra_pcib_port *port;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	unit = fdt_get_unit(dev);
	mtx_init(&sc->mtx, "msi_mtx", NULL, MTX_DEF);


	node = ofw_bus_get_node(dev);

	rv = tegra_pcib_parse_fdt_resources(sc, node);
	if (rv != 0) {
		device_printf(dev, "Cannot get FDT resources\n");
		return (rv);
	}

	nranges = tegra_pci_get_ranges(node, &ranges);
	if (nranges != 5) {
		device_printf(sc->dev, "Unexpected number of ranges: %d\n",
		    nranges);
		rv = ENXIO;
		goto out;
	}

	/* Allocate bus_space resources. */
	rid = 0;
	sc->pads_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->pads_mem_res == NULL) {
		device_printf(dev, "Cannot allocate PADS register\n");
		rv = ENXIO;
		goto out;
	}
	/*
	 * XXX - FIXME
	 * tag for config space is not filled when RF_ALLOCATED flag is used.
	 */
	sc->bus_tag = rman_get_bustag(sc->pads_mem_res);

	rid = 1;
	sc->afi_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->afi_mem_res == NULL) {
		device_printf(dev, "Cannot allocate AFI register\n");
		rv = ENXIO;
		goto out;
	}

	rid = 2;
	sc->cfg_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ALLOCATED);
	if (sc->cfg_mem_res == NULL) {
		device_printf(dev, "Cannot allocate config space memory\n");
		rv = ENXIO;
		goto out;
	}
	sc->cfg_base_addr = rman_get_start(sc->cfg_mem_res);


	/* Map RP slots */
	for (i = 0; i < TEGRA_PCIB_MAX_PORTS; i++) {
		if (sc->ports[i] == NULL)
			continue;
		port = sc->ports[i];
		rv = bus_space_map(sc->bus_tag, port->rp_base_addr,
		    port->rp_size, 0, &port->cfg_handle);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot allocate memory for "
			    "port: %d\n", i);
			rv = ENXIO;
			goto out;
		}
	}

	/*
	 * Get PCI interrupt info.
	 */
	ofw_bus_setup_iinfo(node, &sc->pci_iinfo, sizeof(pcell_t));
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resources\n");
		rv = ENXIO;
		goto out;
	}

	rid = 1;
	sc->msi_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate MSI IRQ resources\n");
		rv = ENXIO;
		goto out;
	}

	if (bus_setup_intr(dev, sc->irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
			   tegra_pci_intr, NULL, sc, &sc->intr_cookie)) {
		device_printf(dev, "cannot setup interrupt handler\n");
		rv = ENXIO;
		goto out;
	}

	/* Memory management. */
	rv = tegra_pcib_decode_ranges(sc, ranges, nranges);
	if (rv != 0)
		goto out;

	rv = tegra_pcib_rman_init(sc);
	if (rv != 0)
		goto out;
	free(ranges, M_DEVBUF);
	ranges = NULL;

	/*
	 * Enable PCIE device.
	 */
	rv = tegra_pcib_enable(sc, unit);
	if (rv != 0)
		goto out;
	for (i = 0; i < TEGRA_PCIB_MAX_PORTS; i++) {
		if (sc->ports[i] == NULL)
			continue;
		if (sc->ports[i]->enabled)
			tegra_pcib_port_enable(sc, i);
		else
			tegra_pcib_port_disable(sc, i);
	}

	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));

out:
	if (ranges != NULL)
		free(ranges, M_DEVBUF);

	return (rv);
}


static device_method_t tegra_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			tegra_pcib_probe),
	DEVMETHOD(device_attach,		tegra_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,		tegra_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,		tegra_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,		tegra_pcib_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		tegra_pcib_adjust_resource),
	DEVMETHOD(bus_release_resource,		tegra_pcib_release_resource),
	DEVMETHOD(bus_activate_resource,	tegra_pcib_pcie_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		tegra_pcib_maxslots),
	DEVMETHOD(pcib_read_config,		tegra_pcib_read_config),
	DEVMETHOD(pcib_write_config,		tegra_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,		tegra_pcib_route_interrupt),

#if defined(TEGRA_PCI_MSI)
	DEVMETHOD(pcib_alloc_msi,		tegra_pcib_alloc_msi),
	DEVMETHOD(pcib_release_msi,		tegra_pcib_release_msi),
	DEVMETHOD(pcib_map_msi,			tegra_pcib_map_msi),
#endif

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,		ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,		ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,		ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,		ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,		ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t tegra_pcib_driver = {
	"pcib",
	tegra_pcib_methods,
	sizeof(struct tegra_pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, simplebus, tegra_pcib_driver, pcib_devclass, 0, 0);
