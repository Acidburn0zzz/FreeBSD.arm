/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
__FBSDID("$FreeBSD: head/sys/boot/mips/beri/loader/beri_disk_cfi.c 298230 2016-04-18 23:09:22Z allanjude $");

#include <sys/param.h>

#include <bootstrap.h>
#include <stdarg.h>

#include <stand.h>
#include <disk.h>

#include <cfi.h>

static int	beri_cfi_disk_init(void);
static int	beri_cfi_disk_open(struct open_file *, ...);
static int	beri_cfi_disk_close(struct open_file *);
static void	beri_cfi_disk_cleanup(void);
static int	beri_cfi_disk_strategy(void *, int, daddr_t, size_t, size_t,
		    char *, size_t *);
static void	beri_cfi_disk_print(int);

struct devsw beri_cfi_disk = {
	.dv_name = "cfi",
	.dv_type = DEVT_DISK,
	.dv_init = beri_cfi_disk_init,
	.dv_strategy = beri_cfi_disk_strategy,
	.dv_open = beri_cfi_disk_open,
	.dv_close = beri_cfi_disk_close,
	.dv_ioctl = noioctl,
	.dv_print = beri_cfi_disk_print,
	.dv_cleanup = beri_cfi_disk_cleanup,
};

static int
beri_cfi_disk_init(void)
{

	return (0);
}

static int
beri_cfi_disk_strategy(void *devdata, int flag, daddr_t dblk, size_t offset,
    size_t size, char *buf, size_t *rsizep)
{
	int error;

	if (flag == F_WRITE)
		return (EROFS);
	if (flag != F_READ)
		return (EINVAL);
	if (rsizep != NULL)
		*rsizep = 0;
	error = cfi_read(buf, dblk, size >> 9);
	if (error == 0 && rsizep != NULL)
		*rsizep = size;
	else if (error != 0)
		printf("%s: error %d\n", __func__, error);
	return (error);
}

static int
beri_cfi_disk_open(struct open_file *f, ...)
{
	va_list ap;
	struct disk_devdesc *dev;

	va_start(ap, f);
	dev = va_arg(ap, struct disk_devdesc *);
	va_end(ap);

	if (dev->d_unit != 0)
		return (EIO);
	return (disk_open(dev, cfi_get_mediasize(), cfi_get_sectorsize(), 0));
}

static int
beri_cfi_disk_close(struct open_file *f)
{
	struct disk_devdesc *dev;

	dev = (struct disk_devdesc *)f->f_devdata;
	return (disk_close(dev));
}

static void
beri_cfi_disk_print(int verbose)
{
	struct disk_devdesc dev;
	char line[80];

	sprintf(line, "    cfi%d   CFI flash device\n", 0);
	pager_output(line);
	dev.d_dev = &beri_cfi_disk;
	dev.d_unit = 0;
	dev.d_slice = -1;
	dev.d_partition = -1;
	if (disk_open(&dev, cfi_get_mediasize(),
	    cfi_get_sectorsize(), 0) == 0) {
		sprintf(line, "    cfi%d", 0);
		disk_print(&dev, line, verbose);
		disk_close(&dev);
	}

}

static void
beri_cfi_disk_cleanup(void)
{

	disk_cleanup(&beri_cfi_disk);
}
