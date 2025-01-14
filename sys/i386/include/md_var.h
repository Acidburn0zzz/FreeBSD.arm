/*-
 * Copyright (c) 1995 Bruce D. Evans.
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
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * $FreeBSD: head/sys/i386/include/md_var.h 297399 2016-03-29 19:56:48Z kib $
 */

#ifndef _MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

#include <x86/x86_var.h>

extern	u_int	cyrix_did;
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern	int	has_f00f_bug;
#endif
#ifdef COMPAT_FREEBSD4
extern	int	szfreebsd4_sigcode;
#endif
#ifdef COMPAT_43
extern	int	szosigcode;
#endif
extern	uint32_t *vm_page_dump;

struct	segment_descriptor;
union savefpu;

void	bcopyb(const void *from, void *to, size_t len);
void	cpu_switch_load_gs(void) __asm(__STRING(cpu_switch_load_gs));
void	doreti_iret(void) __asm(__STRING(doreti_iret));
void	doreti_iret_fault(void) __asm(__STRING(doreti_iret_fault));
void	doreti_popl_ds(void) __asm(__STRING(doreti_popl_ds));
void	doreti_popl_ds_fault(void) __asm(__STRING(doreti_popl_ds_fault));
void	doreti_popl_es(void) __asm(__STRING(doreti_popl_es));
void	doreti_popl_es_fault(void) __asm(__STRING(doreti_popl_es_fault));
void	doreti_popl_fs(void) __asm(__STRING(doreti_popl_fs));
void	doreti_popl_fs_fault(void) __asm(__STRING(doreti_popl_fs_fault));
void	finishidentcpu(void);
void	fill_based_sd(struct segment_descriptor *sdp, uint32_t base);
void	i686_pagezero(void *addr);
void	sse2_pagezero(void *addr);
void	init_AMD_Elan_sc520(void);
vm_paddr_t kvtop(void *addr);
void	ppro_reenable_apic(void);
void	setidt(int idx, alias_for_inthand_t *func, int typ, int dpl, int selec);
union savefpu *get_pcb_user_save_td(struct thread *td);
union savefpu *get_pcb_user_save_pcb(struct pcb *pcb);
struct pcb *get_pcb_td(struct thread *td);

#endif /* !_MACHINE_MD_VAR_H_ */
