/*	$FreeBSD: head/sys/powerpc/powerpc/db_interface.c 290434 2015-11-06 04:56:52Z jhibbits $ */
/*	$NetBSD: db_interface.c,v 1.20 2002/05/13 20:30:09 matt Exp $ */
/*	$OpenBSD: db_interface.c,v 1.2 1996/12/28 06:21:50 rahnds Exp $	*/


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <machine/kdb.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#include <ddb/db_command.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>

#include <dev/ofw/openfirm.h>

int
db_read_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *src;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		src = (char *)addr;

		if (size == 4)
			*((int *)data) = *((int *)src);
		else if (size == 2)
			*((short *)data) = *((short *)src);
		else
			while (size-- > 0)
				*data++ = *src++;
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

int
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *dst;
	size_t cnt;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		dst = (char *)addr;
		cnt = size;

		if (size == 4 && (addr & 3) == 0 && ((uintptr_t)data & 3) == 0)
			*((int*)dst) = *((int*)data);
		else
		if (size == 2 && (addr & 1) == 0 && ((uintptr_t)data & 1) == 0)
			*((short*)dst) = *((short*)data);
		else
			while (cnt-- > 0)
				*dst++ = *data++;
		kdb_cpu_sync_icache((void *)addr, size);
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

void
db_show_mdpcpu(struct pcpu *pc)
{

	db_printf("PPC: hwref   = %#zx\n", pc->pc_hwref);
	db_printf("PPC: ipimask = %#x\n", pc->pc_ipimask);
	db_printf("PPC: pir     = %#x\n", pc->pc_pir);
}
