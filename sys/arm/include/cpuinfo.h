/*-
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
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
 *
 * $FreeBSD: head/sys/arm/include/cpuinfo.h 289602 2015-10-19 19:18:02Z ian $
 */

#ifndef	_MACHINE_CPUINFO_H_
#define	_MACHINE_CPUINFO_H_

#include <sys/types.h>

#define CPU_IMPLEMENTER_ARM		0x41
#define CPU_IMPLEMENTER_QCOM		0x51
#define CPU_IMPLEMENTER_MRVL		0x56

/* ARM */
#define CPU_ARCH_ARM1176		0xB76
#define CPU_ARCH_CORTEX_A5		0xC05
#define CPU_ARCH_CORTEX_A7		0xC07
#define CPU_ARCH_CORTEX_A8		0xC08
#define CPU_ARCH_CORTEX_A9		0xC09
#define CPU_ARCH_CORTEX_A12		0xC0D
#define CPU_ARCH_CORTEX_A15		0xC0F
#define CPU_ARCH_CORTEX_A17		0xC11

/* QCOM */
#define CPU_ARCH_KRAIT_300		0x06F

struct cpuinfo {
	/* raw id registers */
	uint32_t midr;
	uint32_t ctr;
	uint32_t tcmtr;
	uint32_t tlbtr;
	uint32_t mpidr;
	uint32_t revidr;
	uint32_t id_pfr0;
	uint32_t id_pfr1;
	uint32_t id_dfr0;
	uint32_t id_afr0;
	uint32_t id_mmfr0;
	uint32_t id_mmfr1;
	uint32_t id_mmfr2;
	uint32_t id_mmfr3;
	uint32_t id_isar0;
	uint32_t id_isar1;
	uint32_t id_isar2;
	uint32_t id_isar3;
	uint32_t id_isar4;
	uint32_t id_isar5;
	uint32_t cbar;

        /* Parsed bits of above registers... */

	/* midr */
	int implementer;
	int revision;
	int architecture;
	int part_number;
	int patch;

	/* id_mmfr0 */
	int outermost_shareability;
	int shareability_levels;
	int auxiliary_registers;
	int innermost_shareability;

	/* id_mmfr1 */
	int mem_barrier;

	/* id_mmfr3 */
	int coherent_walk;
	int maintenance_broadcast;

	/* id_pfr1 */
	int generic_timer_ext;
	int virtualization_ext;
	int security_ext;

	/* L1 cache info */
	int dcache_line_size;
	int dcache_line_mask;
	int icache_line_size;
	int icache_line_mask;
};

extern struct cpuinfo cpuinfo;

void cpuinfo_init(void);
void cpuinfo_get_actlr_modifier(uint32_t *actlr_mask, uint32_t *actlr_set);
#endif	/* _MACHINE_CPUINFO_H_ */
