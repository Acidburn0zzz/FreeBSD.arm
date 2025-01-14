/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
__FBSDID("$FreeBSD: head/sys/arm/altera/socfpga/socfpga_mp.c 296100 2016-02-26 16:04:47Z andrew $");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#define	SCU_PHYSBASE			0xFFFEC000
#define	SCU_SIZE			0x100

#define	SCU_CONTROL_REG			0x00
#define	 SCU_CONTROL_ENABLE		(1 << 0)
#define	SCU_CONFIG_REG			0x04
#define	 SCU_CONFIG_REG_NCPU_MASK	0x03
#define	SCU_CPUPOWER_REG		0x08
#define	SCU_INV_TAGS_REG		0x0c
#define	SCU_DIAG_CONTROL		0x30
#define	 SCU_DIAG_DISABLE_MIGBIT	(1 << 0)
#define	SCU_FILTER_START_REG		0x40
#define	SCU_FILTER_END_REG		0x44
#define	SCU_SECURE_ACCESS_REG		0x50
#define	SCU_NONSECURE_ACCESS_REG	0x54

#define	RSTMGR_PHYSBASE			0xFFD05000
#define	RSTMGR_SIZE			0x100
#define	MPUMODRST			0x10
#define	 MPUMODRST_CPU1			(1 << 1)

#define	RAM_PHYSBASE			0x0
#define	 RAM_SIZE			0x1000

extern char	*mpentry_addr;
static void	socfpga_trampoline(void);

static void
socfpga_trampoline(void)
{

	__asm __volatile(
			"ldr pc, 1f\n"
			".globl mpentry_addr\n"
			"mpentry_addr:\n"
			"1: .space 4\n");
}

void
platform_mp_setmaxid(void)
{
	int hwcpu, ncpu;

	/* If we've already set this don't bother to do it again. */
	if (mp_ncpus != 0)
		return;

	hwcpu = 2;

	ncpu = hwcpu;
	TUNABLE_INT_FETCH("hw.ncpu", &ncpu);
	if (ncpu < 1 || ncpu > hwcpu)
		ncpu = hwcpu;

	mp_ncpus = ncpu;
	mp_maxid = ncpu - 1;
}

void
platform_mp_start_ap(void)
{
	bus_space_handle_t scu, rst, ram;
	int reg;

	if (bus_space_map(fdtbus_bs_tag, SCU_PHYSBASE,
					SCU_SIZE, 0, &scu) != 0)
		panic("Couldn't map the SCU\n");
	if (bus_space_map(fdtbus_bs_tag, RSTMGR_PHYSBASE,
					RSTMGR_SIZE, 0, &rst) != 0)
		panic("Couldn't map the reset manager (RSTMGR)\n");
	if (bus_space_map(fdtbus_bs_tag, RAM_PHYSBASE,
					RAM_SIZE, 0, &ram) != 0)
		panic("Couldn't map the first physram page\n");

	/* Invalidate SCU cache tags */
	bus_space_write_4(fdtbus_bs_tag, scu,
		SCU_INV_TAGS_REG, 0x0000ffff);

	/*
	 * Erratum ARM/MP: 764369 (problems with cache maintenance).
	 * Setting the "disable-migratory bit" in the undocumented SCU
	 * Diagnostic Control Register helps work around the problem.
	 */
	reg = bus_space_read_4(fdtbus_bs_tag, scu, SCU_DIAG_CONTROL);
	reg |= (SCU_DIAG_DISABLE_MIGBIT);
	bus_space_write_4(fdtbus_bs_tag, scu, SCU_DIAG_CONTROL, reg);

	/* Put CPU1 to reset state */
	bus_space_write_4(fdtbus_bs_tag, rst, MPUMODRST, MPUMODRST_CPU1);

	/* Enable the SCU, then clean the cache on this core */
	reg = bus_space_read_4(fdtbus_bs_tag, scu, SCU_CONTROL_REG);
	reg |= (SCU_CONTROL_ENABLE);
	bus_space_write_4(fdtbus_bs_tag, scu, SCU_CONTROL_REG, reg);

	/* Set up trampoline code */
	mpentry_addr = (char *)pmap_kextract((vm_offset_t)mpentry);
	bus_space_write_region_4(fdtbus_bs_tag, ram, 0,
	    (uint32_t *)&socfpga_trampoline, 8);

	dcache_wbinv_poc_all();

	/* Put CPU1 out from reset */
	bus_space_write_4(fdtbus_bs_tag, rst, MPUMODRST, 0);

	armv7_sev();

	bus_space_unmap(fdtbus_bs_tag, scu, SCU_SIZE);
	bus_space_unmap(fdtbus_bs_tag, rst, RSTMGR_SIZE);
	bus_space_unmap(fdtbus_bs_tag, ram, RAM_SIZE);
}
