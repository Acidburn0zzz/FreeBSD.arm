/*-
 * Copyright (c) 2006, 2008 Stanislav Sedov <stas@FreeBSD.org>.
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
__FBSDID("$FreeBSD: head/usr.sbin/cpucontrol/amd.c 236504 2012-06-03 08:30:00Z avg $");

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <sys/cpuctl.h>

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#include "cpucontrol.h"
#include "amd.h"

int
amd_probe(int fd)
{
	char vendor[13];
	int error;
	cpuctl_cpuid_args_t idargs = {
		.level  = 0,
	};

	error = ioctl(fd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		return (1);
	}
	((uint32_t *)vendor)[0] = idargs.data[1];
	((uint32_t *)vendor)[1] = idargs.data[3];
	((uint32_t *)vendor)[2] = idargs.data[2];
	vendor[12] = '\0';
	if (strncmp(vendor, AMD_VENDOR_ID, sizeof(AMD_VENDOR_ID)) != 0)
		return (1);
	return (0);
}

void
amd_update(const char *dev, const char *path)
{
	int fd, devfd;
	unsigned int i;
	struct stat st;
	uint32_t *fw_image;
	amd_fw_header_t *fw_header;
	uint32_t sum;
	uint32_t signature;
	uint32_t *fw_data;
	size_t fw_size;
	cpuctl_cpuid_args_t idargs = {
		.level  = 1,	/* Request signature. */
	};
	cpuctl_update_args_t args;
	int error;

	assert(path);
	assert(dev);

	fd  = -1;
	fw_image = MAP_FAILED;
	devfd = open(dev, O_RDWR);
	if (devfd < 0) {
		WARN(0, "could not open %s for writing", dev);
		return;
	}
	error = ioctl(devfd, CPUCTL_CPUID, &idargs);
	if (error < 0) {
		WARN(0, "ioctl()");
		goto fail;
	}
	signature = idargs.data[0];
	WARNX(2, "found cpu family %#x model %#x "
	    "stepping %#x extfamily %#x extmodel %#x.",
	    (signature >> 8) & 0x0f, (signature >> 4) & 0x0f,
	    (signature >> 0) & 0x0f, (signature >> 20) & 0xff,
	    (signature >> 16) & 0x0f);

	/*
	 * Open the firmware file.
	 */
	fd = open(path, O_RDONLY, 0);
	if (fd < 0) {
		WARN(0, "open(%s)", path);
		goto fail;
	}
	error = fstat(fd, &st);
	if (error != 0) {
		WARN(0, "fstat(%s)", path);
		goto fail;
	}
	if (st.st_size < 0 || (unsigned)st.st_size < sizeof(*fw_header)) {
		WARNX(2, "file too short: %s", path);
		goto fail;
	}
	/*
	 * mmap the whole image.
	 */
	fw_image = (uint32_t *)mmap(NULL, st.st_size, PROT_READ,
	    MAP_PRIVATE, fd, 0);
	if  (fw_image == MAP_FAILED) {
		WARN(0, "mmap(%s)", path);
		goto fail;
	}
	fw_header = (amd_fw_header_t *)fw_image;
	if ((fw_header->magic >> 8) != AMD_MAGIC) {
		WARNX(2, "%s is not a valid amd firmware: version mismatch",
		    path);
		goto fail;
	}
	fw_data = (uint32_t *)(fw_header + 1);
	fw_size = (st.st_size - sizeof(*fw_header)) / sizeof(uint32_t);

	/*
	 * Check the primary checksum.
	 */
	sum = 0;
	for (i = 0; i < fw_size; i++)
		sum += fw_data[i];
	if (sum != fw_header->checksum) {
		WARNX(2, "%s: update data checksum invalid", path);
		goto fail;
	}
	if (signature == fw_header->signature) {
		fprintf(stderr, "%s: updating cpu %s... ", path, dev);

		args.data = fw_image;
		args.size = st.st_size;
		error = ioctl(devfd, CPUCTL_UPDATE, &args);
		if (error < 0) {
			fprintf(stderr, "failed.\n");
			warn("ioctl()");
			goto fail;
		}
		fprintf(stderr, "done.\n");
	}

fail:
	if (fd >= 0)
		close(fd);
	if (devfd >= 0)
		close(devfd);
	if (fw_image != MAP_FAILED)
		if(munmap(fw_image, st.st_size) != 0)
			warn("munmap(%s)", path);
	return;
}
