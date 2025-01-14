/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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
 * $Id$
 */
/*
 * Skeleton of this file was based on respective code for ARM
 * code written by Olivier Houchard.
 */

/*
 * XXXMIPS: This file is hacked from arm/... . XXXMIPS here means this file is
 * experimental and was written for MIPS32 port.
 */
#include "opt_uart.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/mips/rt305x/uart_bus_rt305x.c 292703 2015-12-24 18:40:10Z adrian $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_bus.h>
#include <dev/uart/uart_cpu.h>

#include <mips/rt305x/rt305xreg.h>

#include "uart_if.h"

static int uart_rt305x_probe(device_t dev);

extern struct uart_class uart_rt305x_uart_class;

static device_method_t uart_rt305x_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uart_rt305x_probe),
	DEVMETHOD(device_attach,	uart_bus_attach),
	DEVMETHOD(device_detach,	uart_bus_detach),
	{ 0, 0 }
};

static driver_t uart_rt305x_driver = {
	uart_driver_name,
	uart_rt305x_methods,
	sizeof(struct uart_softc),
};

static int
uart_rt305x_probe(device_t dev)
{
	struct uart_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_class = &uart_rt305x_uart_class;
	sc->sc_bas.regshft = 2;
	sc->sc_bas.bst = mips_bus_space_generic;
	sc->sc_bas.bsh = 
	    MIPS_PHYS_TO_KSEG1(device_get_unit(dev)?UARTLITE_BASE:UART_BASE);

	return (uart_bus_probe(dev, 2, SYSTEM_CLOCK, 0, 0));
}

DRIVER_MODULE(uart, obio, uart_rt305x_driver, uart_devclass, 0, 0);

