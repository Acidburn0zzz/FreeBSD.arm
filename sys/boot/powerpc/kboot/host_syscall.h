/*-
 * Copyright (C) 2014 Nathan Whitehorn
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
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/sys/boot/powerpc/kboot/host_syscall.h 278020 2015-02-01 02:02:50Z nwhitehorn $
 */

#ifndef _HOST_SYSCALL_H
#define _HOST_SYSCALL_H

#include <stand.h>

ssize_t host_read(int fd, void *buf, size_t nbyte);
ssize_t host_write(int fd, const void *buf, size_t nbyte);
ssize_t host_seek(int fd, int64_t offset, int whence);
int host_open(char *path, int flags, int mode);
int host_close(int fd);
void *host_mmap(void *addr, size_t len, int prot, int flags, int fd, int);
#define host_getmem(size) host_mmap(0, size, 3 /* RW */, 0x22 /* ANON */, -1, 0);
struct host_timeval {
	int tv_sec;
	int tv_usec;
};
int host_gettimeofday(struct host_timeval *a, void *b);
int host_select(int nfds, long *readfds, long *writefds, long *exceptfds,
    struct host_timeval *timeout);
int kexec_load(vm_offset_t start, int nsegs, void *segs);
int host_reboot(int, int, int, void *);
int host_getdents(int fd, void *dirp, int count);

#endif
