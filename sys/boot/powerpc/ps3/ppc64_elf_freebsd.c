/*-
 * Copyright (c) 2001 Benno Rice <benno@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/boot/powerpc/ps3/ppc64_elf_freebsd.c 291598 2015-12-01 17:01:27Z nwhitehorn $");

#define __ELF_WORD_SIZE 64

#include <sys/param.h>
#include <sys/linker.h>

#include <machine/metadata.h>
#include <machine/elf.h>

#include <stand.h>

#include "bootstrap.h"

extern char		end[];
extern vm_offset_t	reloc;	/* From <arch>/conf.c */

int
ppc64_elf_loadfile(char *filename, u_int64_t dest,
    struct preloaded_file **result)
{
	int	r;

	r = __elfN(loadfile)(filename, dest, result);
	if (r != 0)
		return (r);

	/*
	 * No need to sync the icache for modules: this will
	 * be done by the kernel after relocation.
	 */
	if (!strcmp((*result)->f_type, "elf kernel"))
		__syncicache((void *) (*result)->f_addr, (*result)->f_size);
	return (0);
}

int
ppc64_elf_exec(struct preloaded_file *fp)
{
	struct file_metadata	*fmp;
	vm_offset_t		mdp;
	Elf_Ehdr		*e;
	int			error;
	int (*entry)(u_long, u_long, u_long, void *, u_long);

	if ((fmp = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL) {
		return(EFTYPE);
	}
	e = (Elf_Ehdr *)&fmp->md_data;
	
	/* Handle function descriptor for ELFv1 kernels */
	if ((e->e_flags & 3) == 2)
		entry = e->e_entry;
	else
		entry = (void *)(uintptr_t)(*(uint64_t *)e->e_entry);

	if ((error = md_load64(fp->f_args, &mdp)) != 0)
		return (error);

	printf("Kernel entry at %p ...\n", entry);

	dev_cleanup();

	entry(0 /* FDT */, 0 /* Phys. mem offset */, 0 /* OF entry */,
	     (void *)mdp, sizeof(mdp));

	panic("exec returned");
}

struct file_format	ppc_elf64 =
{
	ppc64_elf_loadfile,
	ppc64_elf_exec
};
