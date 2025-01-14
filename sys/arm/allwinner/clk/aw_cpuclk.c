/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 * $FreeBSD: head/sys/arm/allwinner/clk/aw_cpuclk.c 299703 2016-05-13 22:28:02Z gonzo $
 */

/*
 * Allwinner CPU clock
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/allwinner/clk/aw_cpuclk.c 299703 2016-05-13 22:28:02Z gonzo $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/extres/clk/clk_mux.h>

#define	CPU_CLK_SRC_SEL_WIDTH	2
#define	CPU_CLK_SRC_SEL_SHIFT	16

static int
aw_cpuclk_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun4i-a10-cpu-clk"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner CPU Clock");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_cpuclk_attach(device_t dev)
{
	struct clk_mux_def def;
	struct clkdom *clkdom;
	bus_addr_t paddr;
	bus_size_t psize;
	phandle_t node;
	int error, ncells, i;
	clk_t clk;

	node = ofw_bus_get_node(dev);

	if (ofw_reg_to_paddr(node, 0, &paddr, &psize, NULL) != 0) {
		device_printf(dev, "cannot parse 'reg' property\n");
		return (ENXIO);
	}

	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0) {
		device_printf(dev, "cannot get clock count\n");
		return (error);
	}

	clkdom = clkdom_create(dev);

	memset(&def, 0, sizeof(def));
	def.clkdef.id = 1;
	def.clkdef.parent_names = malloc(sizeof(char *) * ncells, M_OFWPROP,
	    M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(dev, i, &clk);
		if (error != 0) {
			device_printf(dev, "cannot get clock %d\n", i);
			goto fail;
		}
		def.clkdef.parent_names[i] = clk_get_name(clk);
		clk_release(clk);
	}
	def.clkdef.parent_cnt = ncells;
	def.offset = paddr;
	def.shift = CPU_CLK_SRC_SEL_SHIFT;
	def.width = CPU_CLK_SRC_SEL_WIDTH;

	error = clk_parse_ofw_clk_name(dev, node, &def.clkdef.name);
	if (error != 0) {
		device_printf(dev, "cannot parse clock name\n");
		error = ENXIO;
		goto fail;
	}

	error = clknode_mux_register(clkdom, &def);
	if (error != 0) {
		device_printf(dev, "cannot register mux clock\n");
		error = ENXIO;
		goto fail;
	}

	if (clkdom_finit(clkdom) != 0) {
		device_printf(dev, "cannot finalize clkdom initialization\n");
		error = ENXIO;
		goto fail;
	}

	OF_prop_free(__DECONST(char *, def.clkdef.parent_names));
	OF_prop_free(__DECONST(char *, def.clkdef.name));

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);

fail:
	OF_prop_free(__DECONST(char *, def.clkdef.name));
	return (error);
}

static device_method_t aw_cpuclk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_cpuclk_probe),
	DEVMETHOD(device_attach,	aw_cpuclk_attach),

	DEVMETHOD_END
};

static driver_t aw_cpuclk_driver = {
	"aw_cpuclk",
	aw_cpuclk_methods,
	0
};

static devclass_t aw_cpuclk_devclass;

EARLY_DRIVER_MODULE(aw_cpuclk, simplebus, aw_cpuclk_driver,
    aw_cpuclk_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
