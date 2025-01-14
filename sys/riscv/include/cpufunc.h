/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 * $FreeBSD: head/sys/riscv/include/cpufunc.h 292407 2015-12-17 18:44:30Z br $
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#ifdef _KERNEL

#include <machine/riscvreg.h>

static __inline void
breakpoint(void)
{

	__asm("ebreak");
}

static __inline register_t
intr_disable(void)
{
	uint64_t ret;

	__asm __volatile(
		"csrrci %0, sstatus, 1"
		: "=&r" (ret)
	);

	return (ret & SSTATUS_IE);
}

static __inline void
intr_restore(register_t s)
{

	__asm __volatile(
		"csrs sstatus, %0"
		:: "r" (s)
	);
}

static __inline void
intr_enable(void)
{

	__asm __volatile(
		"csrsi sstatus, 1"
	);
}

static __inline register_t
machine_command(uint64_t cmd, uint64_t arg)
{
	uint64_t res;

	__asm __volatile(
		"mv	t5, %2\n"
		"mv	t6, %1\n"
		"ecall\n"
		"mv	%0, t6" : "=&r"(res) : "r"(arg), "r"(cmd)
	);

	return (res);
}

#define	cpu_nullop()			riscv_nullop()
#define	cpufunc_nullop()		riscv_nullop()
#define	cpu_setttb(a)			riscv_setttb(a)

#define	cpu_tlb_flushID()		riscv_tlb_flushID()
#define	cpu_tlb_flushID_SE(e)		riscv_tlb_flushID_SE(e)

#define	cpu_dcache_wbinv_range(a, s)	riscv_dcache_wbinv_range((a), (s))
#define	cpu_dcache_inv_range(a, s)	riscv_dcache_inv_range((a), (s))
#define	cpu_dcache_wb_range(a, s)	riscv_dcache_wb_range((a), (s))

#define	cpu_idcache_wbinv_range(a, s)	riscv_idcache_wbinv_range((a), (s))
#define	cpu_icache_sync_range(a, s)	riscv_icache_sync_range((a), (s))

void riscv_nullop(void);
void riscv_setttb(vm_offset_t);
void riscv_tlb_flushID(void);
void riscv_tlb_flushID_SE(vm_offset_t);
void riscv_icache_sync_range(vm_offset_t, vm_size_t);
void riscv_idcache_wbinv_range(vm_offset_t, vm_size_t);
void riscv_dcache_wbinv_range(vm_offset_t, vm_size_t);
void riscv_dcache_inv_range(vm_offset_t, vm_size_t);
void riscv_dcache_wb_range(vm_offset_t, vm_size_t);

#endif	/* _KERNEL */
#endif	/* _MACHINE_CPUFUNC_H_ */
