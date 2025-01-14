/*-
 * Copyright (c) 2003 The FreeBSD Project
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
__FBSDID("$FreeBSD: head/sys/powerpc/powerpc/uma_machdep.c 287015 2015-08-22 07:27:06Z jhibbits $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>
#include <vm/uma.h>
#include <vm/uma_int.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>

static int hw_uma_mdpages;
SYSCTL_INT(_hw, OID_AUTO, uma_mdpages, CTLFLAG_RD, &hw_uma_mdpages, 0,
	   "UMA MD pages in use");

void *
uma_small_alloc(uma_zone_t zone, vm_size_t bytes, u_int8_t *flags, int wait)
{
	void *va;
	vm_paddr_t pa;
	vm_page_t m;
	int pflags;
	
	*flags = UMA_SLAB_PRIV;
	pflags = malloc2vm_flags(wait) | VM_ALLOC_WIRED;

	for (;;) {
		m = vm_page_alloc(NULL, 0, pflags | VM_ALLOC_NOOBJ);
		if (m == NULL) {
			if (wait & M_NOWAIT)
				return (NULL);
			VM_WAIT;
		} else
			break;
	}

	pa = VM_PAGE_TO_PHYS(m);

	/* On book-e sizeof(void *) < sizeof(vm_paddr_t) */
	if ((vm_offset_t)pa != pa)
		return (NULL);

	va = (void *)(vm_offset_t)pa;

	if (!hw_direct_map)
		pmap_kenter((vm_offset_t)va, VM_PAGE_TO_PHYS(m));

	if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
		bzero(va, PAGE_SIZE);
	atomic_add_int(&hw_uma_mdpages, 1);

	return (va);
}

void
uma_small_free(void *mem, vm_size_t size, u_int8_t flags)
{
	vm_page_t m;

	if (!hw_direct_map)
		pmap_remove(kernel_pmap,(vm_offset_t)mem,
		    (vm_offset_t)mem + PAGE_SIZE);

	m = PHYS_TO_VM_PAGE((vm_offset_t)mem);
	m->wire_count--;
	vm_page_free(m);
	atomic_subtract_int(&vm_cnt.v_wire_count, 1);
	atomic_subtract_int(&hw_uma_mdpages, 1);
}
