/*-
 * Copyright (c) 2012 Semihalf.
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
__FBSDID("$FreeBSD: head/sys/dev/fdt/fdt_slicer.c 287013 2015-08-22 05:50:18Z jhibbits $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/slicer.h>

#include <dev/fdt/fdt_common.h>

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

int
fdt_flash_fill_slices(device_t dev, struct flash_slice *slices, int *slices_num)
{
	char *slice_name;
	phandle_t dt_node, dt_child;
	u_long base, size;
	int i;
	ssize_t name_len;

	/*
	 * We assume the caller provides buffer for FLASH_SLICES_MAX_NUM
	 * flash_slice structures.
	 */
	if (slices == NULL) {
		*slices_num = 0;
		return (ENOMEM);
	}

	dt_node = ofw_bus_get_node(dev);
	for (dt_child = OF_child(dt_node), i = 0; dt_child != 0;
	    dt_child = OF_peer(dt_child)) {

		if (i == FLASH_SLICES_MAX_NUM) {
			debugf("not enough buffer for slice i=%d\n", i);
			break;
		}

		/*
		 * Retrieve start and size of the slice.
		 */
		if (fdt_regsize(dt_child, &base, &size) != 0) {
			debugf("error during processing reg property, i=%d\n",
			    i);
			continue;
		}

		if (size == 0) {
			debugf("slice i=%d with no size\n", i);
			continue;
		}

		/*
		 * Retrieve label.
		 */
		name_len = OF_getprop_alloc(dt_child, "label", sizeof(char),
		    (void **)&slice_name);
		if (name_len <= 0) {
			/* Use node name if no label defined */
			name_len = OF_getprop_alloc(dt_child, "name", sizeof(char),
			    (void **)&slice_name);
			if (name_len <= 0) {
				debugf("slice i=%d with no name\n", i);
				slice_name = NULL;
			}
		}

		/*
		 * Fill slice entry data.
		 */
		slices[i].base = base;
		slices[i].size = size;
		slices[i].label = slice_name;
		i++;
	}

	*slices_num = i;
	return (0);
}
