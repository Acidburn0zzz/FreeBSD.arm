/*-
 * Copyright (c) 2009-2012 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/ahci/ahci_generic.c 291689 2015-12-03 11:24:11Z andrew $");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ahci/ahci.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static struct ofw_compat_data compat_data[] = {
	{"generic-ahci", 	1},
	{"snps,dwc-ahci",	1},
	{NULL,			0}
};

static int
ahci_gen_ctlr_reset(device_t dev)
{

	return ahci_ctlr_reset(dev);
}

static int
ahci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc_copy(dev, "AHCI SATA controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ahci_gen_attach(device_t dev)
{
	struct ahci_controller *ctlr = device_get_softc(dev);
	int	error;

	ctlr->r_rid = 0;
	ctlr->r_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &ctlr->r_rid,
	    RF_ACTIVE);
	if (ctlr->r_mem == NULL)
		return (ENXIO);

	/* Setup controller defaults. */
	ctlr->numirqs = 1;

	/* Reset controller */
	if ((error = ahci_gen_ctlr_reset(dev)) == 0)
		error = ahci_attach(dev);

	if (error != 0) {
		if (ctlr->r_mem != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, ctlr->r_rid,
			    ctlr->r_mem);
	}
	return error;
}

static int
ahci_gen_detach(device_t dev)
{

	ahci_detach(dev);
	return (0);
}

static devclass_t ahci_gen_devclass;
static device_method_t ahci_methods[] = {
	DEVMETHOD(device_probe,     ahci_probe),
	DEVMETHOD(device_attach,    ahci_gen_attach),
	DEVMETHOD(device_detach,    ahci_gen_detach),
	DEVMETHOD(bus_print_child,  ahci_print_child),
	DEVMETHOD(bus_alloc_resource,       ahci_alloc_resource),
	DEVMETHOD(bus_release_resource,     ahci_release_resource),
	DEVMETHOD(bus_setup_intr,   ahci_setup_intr),
	DEVMETHOD(bus_teardown_intr,ahci_teardown_intr),
	DEVMETHOD(bus_child_location_str, ahci_child_location_str),
	DEVMETHOD(bus_get_dma_tag,  ahci_get_dma_tag),
	DEVMETHOD_END
};
static driver_t ahci_driver = {
	"ahci",
	ahci_methods,
	sizeof(struct ahci_controller)
};
DRIVER_MODULE(ahci, simplebus, ahci_driver, ahci_gen_devclass, NULL, NULL);
