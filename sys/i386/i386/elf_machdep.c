/*-
 * Copyright 1996-1998 John D. Polstra.
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
__FBSDID("$FreeBSD: head/sys/i386/i386/elf_machdep.c 293613 2016-01-09 20:18:53Z dchagin $");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/imgact_elf.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>

#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/npx.h>

#if !defined(CPU_DISABLE_SSE) && defined(I686_CPU)
#define CPU_ENABLE_SSE
#endif

struct sysentvec elf32_freebsd_sysvec = {
	.sv_size	= SYS_MAXSYSCALL,
	.sv_table	= sysent,
	.sv_mask	= 0,
	.sv_errsize	= 0,
	.sv_errtbl	= NULL,
	.sv_transtrap	= NULL,
	.sv_fixup	= __elfN(freebsd_fixup),
	.sv_sendsig	= sendsig,
	.sv_sigcode	= sigcode,
	.sv_szsigcode	= &szsigcode,
	.sv_name	= "FreeBSD ELF32",
	.sv_coredump	= __elfN(coredump),
	.sv_imgact_try	= NULL,
	.sv_minsigstksz	= MINSIGSTKSZ,
	.sv_pagesize	= PAGE_SIZE,
	.sv_minuser	= VM_MIN_ADDRESS,
	.sv_maxuser	= VM_MAXUSER_ADDRESS,
	.sv_usrstack	= USRSTACK,
	.sv_psstrings	= PS_STRINGS,
	.sv_stackprot	= VM_PROT_ALL,
	.sv_copyout_strings	= exec_copyout_strings,
	.sv_setregs	= exec_setregs,
	.sv_fixlimit	= NULL,
	.sv_maxssiz	= NULL,
	.sv_flags	= SV_ABI_FREEBSD | SV_IA32 | SV_ILP32 | SV_SHP |
			    SV_TIMEKEEP,
	.sv_set_syscall_retval = cpu_set_syscall_retval,
	.sv_fetch_syscall_args = cpu_fetch_syscall_args,
	.sv_syscallnames = syscallnames,
	.sv_shared_page_base = SHAREDPAGE,
	.sv_shared_page_len = PAGE_SIZE,
	.sv_schedtail	= NULL,
	.sv_thread_detach = NULL,
	.sv_trap	= NULL,
};
INIT_SYSENTVEC(elf32_sysvec, &elf32_freebsd_sysvec);

static Elf32_Brandinfo freebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_386,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf32_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_FIRST,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&freebsd_brand_info);

static Elf32_Brandinfo freebsd_brand_oinfo = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_386,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/usr/libexec/ld-elf.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf32_freebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE
};

SYSINIT(oelf32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&freebsd_brand_oinfo);

static Elf32_Brandinfo kfreebsd_brand_info = {
	.brand		= ELFOSABI_FREEBSD,
	.machine	= EM_386,
	.compat_3_brand	= "FreeBSD",
	.emul_path	= NULL,
	.interp_path	= "/lib/ld.so.1",
	.sysvec		= &elf32_freebsd_sysvec,
	.interp_newpath	= NULL,
	.brand_note	= &elf32_kfreebsd_brandnote,
	.flags		= BI_CAN_EXEC_DYN | BI_BRAND_NOTE_MANDATORY
};

SYSINIT(kelf32, SI_SUB_EXEC, SI_ORDER_ANY,
	(sysinit_cfunc_t) elf32_insert_brand_entry,
	&kfreebsd_brand_info);


void
elf32_dump_thread(struct thread *td, void *dst, size_t *off)
{
#ifdef CPU_ENABLE_SSE
	void *buf;
#endif
	size_t len;

	len = 0;
#ifdef CPU_ENABLE_SSE
	if (use_xsave) {
		if (dst != NULL) {
			npxgetregs(td);
			len += elf32_populate_note(NT_X86_XSTATE,
			    get_pcb_user_save_td(td), dst,
			    cpu_max_ext_state_size, &buf);
			*(uint64_t *)((char *)buf + X86_XSTATE_XCR0_OFFSET) =
			    xsave_mask;
		} else
			len += elf32_populate_note(NT_X86_XSTATE, NULL, NULL,
			    cpu_max_ext_state_size, NULL);
	}
#endif
	*off = len;
}

/* Process one elf relocation with addend. */
static int
elf_reloc_internal(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, int local, elf_lookup_fn lookup)
{
	Elf_Addr *where;
	Elf_Addr addr;
	Elf_Addr addend;
	Elf_Word rtype, symidx;
	const Elf_Rel *rel;
	const Elf_Rela *rela;
	int error;

	switch (type) {
	case ELF_RELOC_REL:
		rel = (const Elf_Rel *)data;
		where = (Elf_Addr *) (relocbase + rel->r_offset);
		addend = *where;
		rtype = ELF_R_TYPE(rel->r_info);
		symidx = ELF_R_SYM(rel->r_info);
		break;
	case ELF_RELOC_RELA:
		rela = (const Elf_Rela *)data;
		where = (Elf_Addr *) (relocbase + rela->r_offset);
		addend = rela->r_addend;
		rtype = ELF_R_TYPE(rela->r_info);
		symidx = ELF_R_SYM(rela->r_info);
		break;
	default:
		panic("unknown reloc type %d\n", type);
	}

	if (local) {
		if (rtype == R_386_RELATIVE) {	/* A + B */
			addr = elf_relocaddr(lf, relocbase + addend);
			if (*where != addr)
				*where = addr;
		}
		return (0);
	}

	switch (rtype) {

		case R_386_NONE:	/* none */
			break;

		case R_386_32:		/* S + A */
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return -1;
			addr += addend;
			if (*where != addr)
				*where = addr;
			break;

		case R_386_PC32:	/* S + A - P */
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return -1;
			addr += addend - (Elf_Addr)where;
			if (*where != addr)
				*where = addr;
			break;

		case R_386_COPY:	/* none */
			/*
			 * There shouldn't be copy relocations in kernel
			 * objects.
			 */
			printf("kldload: unexpected R_COPY relocation\n");
			return -1;
			break;

		case R_386_GLOB_DAT:	/* S */
			error = lookup(lf, symidx, 1, &addr);
			if (error != 0)
				return -1;
			if (*where != addr)
				*where = addr;
			break;

		case R_386_RELATIVE:
			break;

		default:
			printf("kldload: unexpected relocation type %d\n",
			       rtype);
			return -1;
	}
	return(0);
}

int
elf_reloc(linker_file_t lf, Elf_Addr relocbase, const void *data, int type,
    elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 0, lookup));
}

int
elf_reloc_local(linker_file_t lf, Elf_Addr relocbase, const void *data,
    int type, elf_lookup_fn lookup)
{

	return (elf_reloc_internal(lf, relocbase, data, type, 1, lookup));
}

int
elf_cpu_load_file(linker_file_t lf __unused)
{

	return (0);
}

int
elf_cpu_unload_file(linker_file_t lf __unused)
{

	return (0);
}
