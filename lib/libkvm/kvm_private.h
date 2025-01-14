/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)kvm_private.h	8.1 (Berkeley) 6/4/93
 * $FreeBSD: head/lib/libkvm/kvm_private.h 291406 2015-11-27 18:58:26Z jhb $
 */

#include <sys/endian.h>
#include <sys/linker_set.h>
#include <gelf.h>

struct kvm_arch {
	int	(*ka_probe)(kvm_t *);
	int	(*ka_initvtop)(kvm_t *);
	void	(*ka_freevtop)(kvm_t *);
	int	(*ka_kvatop)(kvm_t *, kvaddr_t, off_t *);
	int	(*ka_native)(kvm_t *);
};

#define	KVM_ARCH(ka)	DATA_SET(kvm_arch, ka)

struct __kvm {
	struct kvm_arch *arch;
	/*
	 * a string to be prepended to error messages
	 * provided for compatibility with sun's interface
	 * if this value is null, errors are saved in errbuf[]
	 */
	const char *program;
	char	*errp;		/* XXX this can probably go away */
	char	errbuf[_POSIX2_LINE_MAX];
#define ISALIVE(kd) ((kd)->vmfd >= 0)
	int	pmfd;		/* physical memory file (or crashdump) */
	int	vmfd;		/* virtual memory file (-1 if crashdump) */
	int	nlfd;		/* namelist file (e.g., /kernel) */
	GElf_Ehdr nlehdr;	/* ELF file header for namelist file */
	int	(*resolve_symbol)(const char *, kvaddr_t *);
	struct kinfo_proc *procbase;
	char	*argspc;	/* (dynamic) storage for argv strings */
	int	arglen;		/* length of the above */
	char	**argv;		/* (dynamic) storage for argv pointers */
	int	argc;		/* length of above (not actual # present) */
	char	*argbuf;	/* (dynamic) temporary storage */
	/*
	 * Kernel virtual address translation state.  This only gets filled
	 * in for dead kernels; otherwise, the running kernel (i.e. kmem)
	 * will do the translations for us.  It could be big, so we
	 * only allocate it if necessary.
	 */
	struct vmstate *vmst;
	int	rawdump;	/* raw dump format */

	int		vnet_initialized;	/* vnet fields set up */
	kvaddr_t	vnet_start;	/* start of kernel's vnet region */
	kvaddr_t	vnet_stop;	/* stop of kernel's vnet region */
	kvaddr_t	vnet_current;	/* vnet we're working with */
	kvaddr_t	vnet_base;	/* vnet base of current vnet */

	/*
	 * Dynamic per-CPU kernel memory.  We translate symbols, on-demand,
	 * to the data associated with dpcpu_curcpu, set with
	 * kvm_dpcpu_setcpu().
	 */
	int		dpcpu_initialized;	/* dpcpu fields set up */
	kvaddr_t	dpcpu_start;	/* start of kernel's dpcpu region */
	kvaddr_t	dpcpu_stop;	/* stop of kernel's dpcpu region */
	u_int		dpcpu_maxcpus;	/* size of base array */
	uintptr_t	*dpcpu_off;	/* base array, indexed by CPU ID */
	u_int		dpcpu_curcpu;	/* CPU we're currently working with */
	kvaddr_t	dpcpu_curoff;	/* dpcpu base of current CPU */
};

/*
 * Page table hash used by minidump backends to map physical addresses
 * to file offsets.
 */
struct hpte {
	struct hpte	*next;
	uint64_t	pa;
	off_t		off;
};

#define HPT_SIZE 1024

struct hpt {
	struct hpte	*hpt_head[HPT_SIZE];
};

/*
 * Functions used internally by kvm, but across kvm modules.
 */
static inline uint32_t
_kvm32toh(kvm_t *kd, uint32_t val)
{

	if (kd->nlehdr.e_ident[EI_DATA] == ELFDATA2LSB)
		return (le32toh(val));
	else
		return (be32toh(val));
}

static inline uint64_t
_kvm64toh(kvm_t *kd, uint64_t val)
{

	if (kd->nlehdr.e_ident[EI_DATA] == ELFDATA2LSB)
		return (le64toh(val));
	else
		return (be64toh(val));
}

void	 _kvm_err(kvm_t *kd, const char *program, const char *fmt, ...)
	    __printflike(3, 4);
void	 _kvm_freeprocs(kvm_t *kd);
void	*_kvm_malloc(kvm_t *kd, size_t);
int	 _kvm_nlist(kvm_t *, struct kvm_nlist *, int);
void	*_kvm_realloc(kvm_t *kd, void *, size_t);
void	 _kvm_syserr (kvm_t *kd, const char *program, const char *fmt, ...)
	    __printflike(3, 4);
int	 _kvm_vnet_selectpid(kvm_t *, pid_t);
int	 _kvm_vnet_initialized(kvm_t *, int);
kvaddr_t _kvm_vnet_validaddr(kvm_t *, kvaddr_t);
int	 _kvm_dpcpu_initialized(kvm_t *, int);
kvaddr_t _kvm_dpcpu_validaddr(kvm_t *, kvaddr_t);
int	 _kvm_probe_elf_kernel(kvm_t *, int, int);
int	 _kvm_is_minidump(kvm_t *);
int	 _kvm_read_core_phdrs(kvm_t *, size_t *, GElf_Phdr **);
void	 _kvm_hpt_init(kvm_t *, struct hpt *, void *, size_t, off_t, int, int);
off_t	 _kvm_hpt_find(struct hpt *, uint64_t);
void	 _kvm_hpt_free(struct hpt *);
