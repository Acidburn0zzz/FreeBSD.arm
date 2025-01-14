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
 * $FreeBSD: head/sys/arm/allwinner/aw_sid.c 299871 2016-05-15 22:36:55Z jmcneill $
 */

/*
 * Allwinner secure ID controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/allwinner/aw_sid.c 299871 2016-05-15 22:36:55Z jmcneill $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/aw_sid.h>

#define	SID_SRAM		0x200
#define	SID_THERMAL_CALIB0	(SID_SRAM + 0x34)
#define	SID_THERMAL_CALIB1	(SID_SRAM + 0x38)

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun8i-a83t-sid",		1 },
	{ NULL,					0 }
};

struct aw_sid_softc {
	struct resource		*res;
};

static struct aw_sid_softc *aw_sid_sc;

static struct resource_spec aw_sid_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int
aw_sid_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Secure ID Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_sid_attach(device_t dev)
{
	struct aw_sid_softc *sc;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, aw_sid_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	aw_sid_sc = sc;

	return (0);
}

int
aw_sid_read_tscalib(uint32_t *calib0, uint32_t *calib1)
{
	struct aw_sid_softc *sc;

	sc = aw_sid_sc;
	if (sc == NULL)
		return (ENXIO);

	*calib0 = RD4(sc, SID_THERMAL_CALIB0);
	*calib1 = RD4(sc, SID_THERMAL_CALIB1);

	return (0);
}

static device_method_t aw_sid_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_sid_probe),
	DEVMETHOD(device_attach,	aw_sid_attach),

	DEVMETHOD_END
};

static driver_t aw_sid_driver = {
	"aw_sid",
	aw_sid_methods,
	sizeof(struct aw_sid_softc),
};

static devclass_t aw_sid_devclass;

EARLY_DRIVER_MODULE(aw_sid, simplebus, aw_sid_driver, aw_sid_devclass, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(aw_sid, 1);
