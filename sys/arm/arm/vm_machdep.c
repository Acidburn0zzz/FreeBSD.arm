/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1994 John Dyson
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
 *
 * Redistribution and use in source and binary :forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 */

#include "opt_compat.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/arm/arm/vm_machdep.c 301961 2016-06-16 12:05:44Z kib $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/unistd.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>
#include <machine/vfp.h>

/*
 * struct switchframe and trapframe must both be a multiple of 8
 * for correct stack alignment.
 */
CTASSERT(sizeof(struct switchframe) == 48);
CTASSERT(sizeof(struct trapframe) == 80);

uint32_t initial_fpscr = VFPSCR_DN | VFPSCR_FZ;

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(register struct thread *td1, register struct proc *p2,
    struct thread *td2, int flags)
{
	struct pcb *pcb2;
	struct trapframe *tf;
	struct mdproc *mdp2;

	if ((flags & RFPROC) == 0)
		return;

	/* Point the pcb to the top of the stack */
	pcb2 = (struct pcb *)
	    (td2->td_kstack + td2->td_kstack_pages * PAGE_SIZE) - 1;
#ifdef __XSCALE__
#ifndef CPU_XSCALE_CORE3
	pmap_use_minicache(td2->td_kstack, td2->td_kstack_pages * PAGE_SIZE);
#endif
#endif
	td2->td_pcb = pcb2;

	/* Clone td1's pcb */
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	/* Point to mdproc and then copy over td1's contents */
	mdp2 = &p2->p_md;
	bcopy(&td1->td_proc->p_md, mdp2, sizeof(*mdp2));

	/* Point the frame to the stack in front of pcb and copy td1's frame */
	td2->td_frame = (struct trapframe *)pcb2 - 1;
	*td2->td_frame = *td1->td_frame;

	/*
	 * Create a new fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 */
	pmap_set_pcb_pagedir(vmspace_pmap(p2->p_vmspace), pcb2);
	pcb2->pcb_regs.sf_r4 = (register_t)fork_return;
	pcb2->pcb_regs.sf_r5 = (register_t)td2;
	pcb2->pcb_regs.sf_lr = (register_t)fork_trampoline;
	pcb2->pcb_regs.sf_sp = STACKALIGN(td2->td_frame);

	pcb2->pcb_vfpcpu = -1;
	pcb2->pcb_vfpstate.fpscr = initial_fpscr;

	tf = td2->td_frame;
	tf->tf_spsr &= ~PSR_C;
	tf->tf_r0 = 0;
	tf->tf_r1 = 0;


	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_cspr = PSR_SVC32_MODE;
#if __ARM_ARCH >= 6
	td2->td_md.md_tp = td1->td_md.md_tp;
#else
	td2->td_md.md_tp = *(register_t *)ARM_TP_ADDRESS;
#endif
}

void
cpu_thread_swapin(struct thread *td)
{
}

void
cpu_thread_swapout(struct thread *td)
{
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;
	int fixup;
#ifdef __ARMEB__
	u_int call;
#endif

	frame = td->td_frame;
	fixup = 0;

#ifdef __ARMEB__
	/*
	 * __syscall returns an off_t while most other syscalls return an
	 * int. As an off_t is 64-bits and an int is 32-bits we need to
	 * place the returned data into r1. As the lseek and freebsd6_lseek
	 * syscalls also return an off_t they do not need this fixup.
	 */
	call = frame->tf_r7;
	if (call == SYS___syscall) {
		register_t *ap = &frame->tf_r0;
		register_t code = ap[_QUAD_LOWWORD];
		if (td->td_proc->p_sysent->sv_mask)
			code &= td->td_proc->p_sysent->sv_mask;
		fixup = (code != SYS_lseek);
	}
#endif

	switch (error) {
	case 0:
		if (fixup) {
			frame->tf_r0 = 0;
			frame->tf_r1 = td->td_retval[0];
		} else {
			frame->tf_r0 = td->td_retval[0];
			frame->tf_r1 = td->td_retval[1];
		}
		frame->tf_spsr &= ~PSR_C;   /* carry bit */
		break;
	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
#if __ARM_ARCH >= 7
		if ((frame->tf_spsr & PSR_T) != 0)
			frame->tf_pc -= THUMB_INSN_SIZE;
		else
#endif
			frame->tf_pc -= INSN_SIZE;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
		frame->tf_r0 = error;
		frame->tf_spsr |= PSR_C;    /* carry bit */
		break;
	}
}

/*
 * Initialize machine state, mostly pcb and trap frame for a new
 * thread, about to return to userspace.  Put enough state in the new
 * thread's PCB to get it to go back to the fork_return(), which
 * finalizes the thread state and handles peculiarities of the first
 * return to userspace for the new thread.
 */
void
cpu_copy_thread(struct thread *td, struct thread *td0)
{

	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));
	bcopy(td0->td_pcb, td->td_pcb, sizeof(struct pcb));

	td->td_pcb->pcb_regs.sf_r4 = (register_t)fork_return;
	td->td_pcb->pcb_regs.sf_r5 = (register_t)td;
	td->td_pcb->pcb_regs.sf_lr = (register_t)fork_trampoline;
	td->td_pcb->pcb_regs.sf_sp = STACKALIGN(td->td_frame);

	td->td_frame->tf_spsr &= ~PSR_C;
	td->td_frame->tf_r0 = 0;

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_cspr = PSR_SVC32_MODE;
}

/*
 * Set that machine state for performing an upcall that starts
 * the entry function with the given argument.
 */
void
cpu_set_upcall(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
	struct trapframe *tf = td->td_frame;

	tf->tf_usr_sp = STACKALIGN((int)stack->ss_sp + stack->ss_size);
	tf->tf_pc = (int)entry;
	tf->tf_r0 = (int)arg;
	tf->tf_spsr = PSR_USR32_MODE;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	td->td_md.md_tp = (register_t)tls_base;
	if (td == curthread) {
		critical_enter();
#if __ARM_ARCH >= 6
		set_tls(tls_base);
#else
		*(register_t *)ARM_TP_ADDRESS = (register_t)tls_base;
#endif
		critical_exit();
	}
	return (0);
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	td->td_pcb = (struct pcb *)(td->td_kstack + td->td_kstack_pages *
	    PAGE_SIZE) - 1;
	/*
	 * Ensure td_frame is aligned to an 8 byte boundary as it will be
	 * placed into the stack pointer which must be 8 byte aligned in
	 * the ARM EABI.
	 */
	td->td_frame = (struct trapframe *)((caddr_t)td->td_pcb) - 1;

#ifdef __XSCALE__
#ifndef CPU_XSCALE_CORE3
	pmap_use_minicache(td->td_kstack, td->td_kstack_pages * PAGE_SIZE);
#endif
#endif
}

void
cpu_thread_free(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_fork_kthread_handler(struct thread *td, void (*func)(void *), void *arg)
{
	td->td_pcb->pcb_regs.sf_r4 = (register_t)func;	/* function */
	td->td_pcb->pcb_regs.sf_r5 = (register_t)arg;	/* first arg */
}

/*
 * Software interrupt handler for queued VM system processing.
 */
void
swi_vm(void *dummy)
{

	if (busdma_swi_pending)
		busdma_swi();
}

void
cpu_exit(struct thread *td)
{
}

