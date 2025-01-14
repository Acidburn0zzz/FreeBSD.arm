/*-
 * Copyright (c) 2015 Justin Hibbits
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/powerpc/mikrotik/platform_rb.c 287013 2015-08-22 05:50:18Z jhibbits $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/slicer.h>
#include <sys/smp.h>

#include <machine/platform.h>
#include <machine/platformvar.h>

#include <dev/nand/nand.h>
#include <dev/ofw/openfirm.h>
#include <geom/geom_disk.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include "platform_if.h"

static int rb_probe(platform_t);
static int rb_attach(platform_t);

static platform_method_t rb_methods[] = {
	PLATFORMMETHOD(platform_probe,		rb_probe),
	PLATFORMMETHOD(platform_attach,		rb_attach),
	PLATFORMMETHOD_END
};

DEFINE_CLASS_1(rb, rb_platform, rb_methods, 0, mpc85xx_platform);

PLATFORM_DEF(rb_platform);

/* Slicer operates on the NAND controller, so we have to find the chip. */
static int
rb_nand_slicer(device_t dev, struct flash_slice *slices, int *nslices)
{
	struct nand_chip *chip;
	device_t *children;
	int n;

	if (device_get_children(dev, &children, &n) != 0) {
		panic("Slicer called on controller with no child!");
	}
	dev = children[0];
	free(children, M_TEMP);

	if (device_get_children(dev, &children, &n) != 0) {
		panic("Slicer called on controller with nandbus but no child!");
	}
	dev = children[0];
	free(children, M_TEMP);

	chip = device_get_softc(dev);
	*nslices = 2;
	slices[0].base = 0;
	slices[0].size = 4 * 1024 * 1024;
	slices[0].label = "boot";

	slices[1].base = 4 * 1024 * 1024;
	slices[1].size = chip->ndisk->d_mediasize - slices[0].size;
	slices[1].label = "rootfs";

	return (0);
}

static int
rb_probe(platform_t plat)
{
	phandle_t rootnode;
	char model[32];

	rootnode = OF_finddevice("/");

	if (OF_getprop(rootnode, "model", model, sizeof(model)) > 0) {
		if (strcmp(model, "RB800") == 0)
			return (BUS_PROBE_SPECIFIC);
	}

	return (ENXIO);
}

static int
rb_attach(platform_t plat)
{
	int error;

	error = mpc85xx_attach(plat);
	if (error)
		return (error);

	flash_register_slicer(rb_nand_slicer);

	return (0);
}
