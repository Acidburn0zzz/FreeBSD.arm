/*-
 * Copyright (c) 2014 Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/at91/at91_shdwc.c 262599 2014-02-28 03:00:25Z imp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_aicreg.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

struct shdwc_softc {
	struct resource	*mem_res;	/* Memory resource */
	device_t	sc_dev;
};

static int
at91_shdwc_probe(device_t dev)
{
#ifdef FDT
	if (!ofw_bus_is_compatible(dev, "atmel,at91sam9260-shdwc"))
		return (ENXIO);
#endif
	device_set_desc(dev, "SHDWC");
        return (0);
}

static int
at91_shdwc_attach(device_t dev)
{
	int rid, err = 0;
	struct shdwc_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (sc->mem_res == NULL)
		panic("couldn't allocate register resources");

	return (err);
}

static device_method_t at91_shdwc_methods[] = {
	DEVMETHOD(device_probe, at91_shdwc_probe),
	DEVMETHOD(device_attach, at91_shdwc_attach),
	DEVMETHOD_END
};

static driver_t at91_shdwc_driver = {
	"at91_shdwc",
	at91_shdwc_methods,
	sizeof(struct shdwc_softc),
};

static devclass_t at91_shdwc_devclass;

#ifdef FDT
DRIVER_MODULE(at91_shdwc, simplebus, at91_shdwc_driver, at91_shdwc_devclass, NULL,
    NULL);
#else
DRIVER_MODULE(at91_shdwc, atmelarm, at91_shdwc_driver, at91_shdwc_devclass, NULL,
    NULL);
#endif
