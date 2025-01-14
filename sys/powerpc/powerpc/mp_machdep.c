/*-
 * Copyright (c) 2008 Marcel Moolenaar
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
__FBSDID("$FreeBSD: stable/11/sys/powerpc/powerpc/mp_machdep.c 302372 2016-07-06 14:09:49Z nwhitehorn $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/pcb.h>
#include <machine/platform.h>
#include <machine/md_var.h>
#include <machine/setjmp.h>
#include <machine/smp.h>

#include "pic_if.h"

extern struct pcpu __pcpu[MAXCPU];

volatile static int ap_awake;
volatile static u_int ap_letgo;
volatile static u_quad_t ap_timebase;
static u_int ipi_msg_cnt[32];
static struct mtx ap_boot_mtx;
struct pcb stoppcbs[MAXCPU];

void
machdep_ap_bootstrap(void)
{

	/* Set PIR */
	PCPU_SET(pir, mfspr(SPR_PIR));
	PCPU_SET(awake, 1);
	__asm __volatile("msync; isync");

	while (ap_letgo == 0)
		;

	/* Initialize DEC and TB, sync with the BSP values */
#ifdef __powerpc64__
	/* Writing to the time base register is hypervisor-privileged */
	if (mfmsr() & PSL_HV)
		mttb(ap_timebase);
#else
	mttb(ap_timebase);
#endif
	decr_ap_init();

	/* Give platform code a chance to do anything necessary */
	platform_smp_ap_init();

	/* Serialize console output and AP count increment */
	mtx_lock_spin(&ap_boot_mtx);
	ap_awake++;
	printf("SMP: AP CPU #%d launched\n", PCPU_GET(cpuid));
	mtx_unlock_spin(&ap_boot_mtx);

	/* Start per-CPU event timers. */
	cpu_initclocks_ap();

	/* Announce ourselves awake, and enter the scheduler */
	sched_throw(NULL);
}

void
cpu_mp_setmaxid(void)
{
	struct cpuref cpuref;
	int error;

	mp_ncpus = 0;
	mp_maxid = 0;
	error = platform_smp_first_cpu(&cpuref);
	while (!error) {
		mp_ncpus++;
		mp_maxid = max(cpuref.cr_cpuid, mp_maxid);
		error = platform_smp_next_cpu(&cpuref);
	}
	/* Sanity. */
	if (mp_ncpus == 0)
		mp_ncpus = 1;
}

int
cpu_mp_probe(void)
{

	/*
	 * We're not going to enable SMP if there's only 1 processor.
	 */
	return (mp_ncpus > 1);
}

void
cpu_mp_start(void)
{
	struct cpuref bsp, cpu;
	struct pcpu *pc;
	int error;

	error = platform_smp_get_bsp(&bsp);
	KASSERT(error == 0, ("Don't know BSP"));
	KASSERT(bsp.cr_cpuid == 0, ("%s: cpuid != 0", __func__));

	error = platform_smp_first_cpu(&cpu);
	while (!error) {
		if (cpu.cr_cpuid >= MAXCPU) {
			printf("SMP: cpu%d: skipped -- ID out of range\n",
			    cpu.cr_cpuid);
			goto next;
		}
		if (CPU_ISSET(cpu.cr_cpuid, &all_cpus)) {
			printf("SMP: cpu%d: skipped - duplicate ID\n",
			    cpu.cr_cpuid);
			goto next;
		}
		if (cpu.cr_cpuid != bsp.cr_cpuid) {
			void *dpcpu;

			pc = &__pcpu[cpu.cr_cpuid];
			dpcpu = (void *)kmem_malloc(kernel_arena, DPCPU_SIZE,
			    M_WAITOK | M_ZERO);
			pcpu_init(pc, cpu.cr_cpuid, sizeof(*pc));
			dpcpu_init(dpcpu, cpu.cr_cpuid);
		} else {
			pc = pcpup;
			pc->pc_cpuid = bsp.cr_cpuid;
			pc->pc_bsp = 1;
		}
		pc->pc_hwref = cpu.cr_hwref;
		CPU_SET(pc->pc_cpuid, &all_cpus);
next:
		error = platform_smp_next_cpu(&cpu);
	}
}

void
cpu_mp_announce(void)
{
	struct pcpu *pc;
	int i;

	for (i = 0; i <= mp_maxid; i++) {
		pc = pcpu_find(i);
		if (pc == NULL)
			continue;
		printf("cpu%d: dev=%x", i, (int)pc->pc_hwref);
		if (pc->pc_bsp)
			printf(" (BSP)");
		printf("\n");
	}
}

static void
cpu_mp_unleash(void *dummy)
{
	struct pcpu *pc;
	int cpus, timeout;

	if (mp_ncpus <= 1)
		return;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	cpus = 0;
	smp_cpus = 0;
#ifdef BOOKE
	tlb1_ap_prep();
#endif
	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpus++;
		if (!pc->pc_bsp) {
			if (bootverbose)
				printf("Waking up CPU %d (dev=%x)\n",
				    pc->pc_cpuid, (int)pc->pc_hwref);

			platform_smp_start_cpu(pc);
			
			timeout = 2000;	/* wait 2sec for the AP */
			while (!pc->pc_awake && --timeout > 0)
				DELAY(1000);

		} else {
			PCPU_SET(pir, mfspr(SPR_PIR));
			pc->pc_awake = 1;
		}
		if (pc->pc_awake) {
			if (bootverbose)
				printf("Adding CPU %d, pir=%x, awake=%x\n",
				    pc->pc_cpuid, pc->pc_pir, pc->pc_awake);
			smp_cpus++;
		} else
			CPU_SET(pc->pc_cpuid, &stopped_cpus);
	}

	ap_awake = 1;

	/* Provide our current DEC and TB values for APs */
	ap_timebase = mftb() + 10;
	__asm __volatile("msync; isync");
	
	/* Let APs continue */
	atomic_store_rel_int(&ap_letgo, 1);

#ifdef __powerpc64__
	/* Writing to the time base register is hypervisor-privileged */
	if (mfmsr() & PSL_HV)
		mttb(ap_timebase);
#else
	mttb(ap_timebase);
#endif

	while (ap_awake < smp_cpus)
		;

	if (smp_cpus != cpus || cpus != mp_ncpus) {
		printf("SMP: %d CPUs found; %d CPUs usable; %d CPUs woken\n",
		    mp_ncpus, cpus, smp_cpus);
	}

	/* Let the APs get into the scheduler */
	DELAY(10000);

	/* XXX Atomic set operation? */
	smp_started = 1;
}

SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, cpu_mp_unleash, NULL);

int
powerpc_ipi_handler(void *arg)
{
	u_int cpuid;
	uint32_t ipimask;
	int msg;

	CTR2(KTR_SMP, "%s: MSR 0x%08x", __func__, mfmsr());

	ipimask = atomic_readandclear_32(&(pcpup->pc_ipimask));
	if (ipimask == 0)
		return (FILTER_STRAY);
	while ((msg = ffs(ipimask) - 1) != -1) {
		ipimask &= ~(1u << msg);
		ipi_msg_cnt[msg]++;
		switch (msg) {
		case IPI_AST:
			CTR1(KTR_SMP, "%s: IPI_AST", __func__);
			break;
		case IPI_PREEMPT:
			CTR1(KTR_SMP, "%s: IPI_PREEMPT", __func__);
			sched_preempt(curthread);
			break;
		case IPI_RENDEZVOUS:
			CTR1(KTR_SMP, "%s: IPI_RENDEZVOUS", __func__);
			smp_rendezvous_action();
			break;
		case IPI_STOP:

			/*
			 * IPI_STOP_HARD is mapped to IPI_STOP so it is not
			 * necessary to add such case in the switch.
			 */
			CTR1(KTR_SMP, "%s: IPI_STOP or IPI_STOP_HARD (stop)",
			    __func__);
			cpuid = PCPU_GET(cpuid);
			savectx(&stoppcbs[cpuid]);
			savectx(PCPU_GET(curpcb));
			CPU_SET_ATOMIC(cpuid, &stopped_cpus);
			while (!CPU_ISSET(cpuid, &started_cpus))
				cpu_spinwait();
			CPU_CLR_ATOMIC(cpuid, &stopped_cpus);
			CPU_CLR_ATOMIC(cpuid, &started_cpus);
			CTR1(KTR_SMP, "%s: IPI_STOP (restart)", __func__);
			break;
		case IPI_HARDCLOCK:
			CTR1(KTR_SMP, "%s: IPI_HARDCLOCK", __func__);
			hardclockintr();
			break;
		}
	}

	return (FILTER_HANDLED);
}

static void
ipi_send(struct pcpu *pc, int ipi)
{

	CTR4(KTR_SMP, "%s: pc=%p, targetcpu=%d, IPI=%d", __func__,
	    pc, pc->pc_cpuid, ipi);

	atomic_set_32(&pc->pc_ipimask, (1 << ipi));
	powerpc_sync();
	PIC_IPI(root_pic, pc->pc_cpuid);

	CTR1(KTR_SMP, "%s: sent", __func__);
}

/* Send an IPI to a set of cpus. */
void
ipi_selected(cpuset_t cpus, int ipi)
{
	struct pcpu *pc;

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (CPU_ISSET(pc->pc_cpuid, &cpus))
			ipi_send(pc, ipi);
	}
}

/* Send an IPI to a specific CPU. */
void
ipi_cpu(int cpu, u_int ipi)
{

	ipi_send(cpuid_to_pcpu[cpu], ipi);
}

/* Send an IPI to all CPUs EXCEPT myself. */
void
ipi_all_but_self(int ipi)
{
	struct pcpu *pc;

	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc != pcpup)
			ipi_send(pc, ipi);
	}
}
