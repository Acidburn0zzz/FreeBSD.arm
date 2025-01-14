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
 * $FreeBSD: head/sys/arm/allwinner/clk/aw_codecclk.c 297627 2016-04-06 23:11:03Z jmcneill $
 */

/*
 * Allwinner CODEC clock
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/allwinner/clk/aw_codecclk.c 297627 2016-04-06 23:11:03Z jmcneill $");

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

#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/hwreset/hwreset.h>

#include "clkdev_if.h"

#define	SCLK_GATING_SHIFT	31

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun4i-a10-codec-clk",	1 },
	{ NULL, 0 }
};

static int
aw_codecclk_create(device_t dev, bus_addr_t paddr, struct clkdom *clkdom,
    const char *pclkname, const char *clkname, int index)
{
	const char *parent_names[1] = { pclkname };
	struct clk_gate_def def;

	memset(&def, 0, sizeof(def));
	def.clkdef.id = index;
	def.clkdef.name = clkname;
	def.clkdef.parent_names = parent_names;
	def.clkdef.parent_cnt = 1;
	def.offset = paddr;
	def.shift = SCLK_GATING_SHIFT;
	def.mask = 1;
	def.on_value = 1;
	def.off_value = 0;

	return (clknode_gate_register(clkdom, &def));
}

static int
aw_codecclk_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner CODEC Clock");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_codecclk_attach(device_t dev)
{
	struct clkdom *clkdom;
	const char **names;
	int nout, error;
	uint32_t *indices;
	clk_t clk_parent;
	bus_addr_t paddr;
	bus_size_t psize;
	phandle_t node;

	node = ofw_bus_get_node(dev);
	indices = NULL;

	if (ofw_reg_to_paddr(node, 0, &paddr, &psize, NULL) != 0) {
		device_printf(dev, "cannot parse 'reg' property\n");
		return (ENXIO);
	}

	clkdom = clkdom_create(dev);

	nout = clk_parse_ofw_out_names(dev, node, &names, &indices);
	if (nout != 1) {
		device_printf(dev, "must have exactly one output clock\n");
		error = ENOENT;
		goto fail;
	}

	error = clk_get_by_ofw_index(dev, 0, &clk_parent);
	if (error != 0) {
		device_printf(dev, "cannot parse clock parent\n");
		return (ENXIO);
	}

	error = aw_codecclk_create(dev, paddr, clkdom,
	    clk_get_name(clk_parent), names[0], 1);

	if (clkdom_finit(clkdom) != 0) {
		device_printf(dev, "cannot finalize clkdom initialization\n");
		error = ENXIO;
		goto fail;
	}

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);

fail:
	return (error);
}

static device_method_t aw_codecclk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_codecclk_probe),
	DEVMETHOD(device_attach,	aw_codecclk_attach),

	DEVMETHOD_END
};

static driver_t aw_codecclk_driver = {
	"aw_codecclk",
	aw_codecclk_methods,
	0
};

static devclass_t aw_codecclk_devclass;

EARLY_DRIVER_MODULE(aw_codecclk, simplebus, aw_codecclk_driver,
    aw_codecclk_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
