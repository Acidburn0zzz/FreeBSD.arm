/*-
 * Copyright (c) 2016 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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
 *
 * $FreeBSD: head/tests/sys/aio/local.h 296277 2016-03-01 18:12:14Z jhb $
 */

#ifndef _AIO_TEST_LOCAL_H_
#define	_AIO_TEST_LOCAL_H_

#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define	ATF_REQUIRE_UNSAFE_AIO() do {					\
	size_t _len;							\
	int _unsafe;							\
									\
	_len = sizeof(_unsafe);						\
	if (sysctlbyname("vfs.aio.enable_unsafe", &_unsafe, &_len, NULL,\
	    0) < 0) {							\
		if (errno != ENOENT)					\
			atf_libc_error(errno,				\
			    "Failed to read vfs.aio.enable_unsafe");	\
	} else if (_unsafe == 0)					\
		atf_tc_skip("Unsafe AIO is disabled");			\
} while (0)
	
#define	PLAIN_REQUIRE_UNSAFE_AIO(_exit_code) do {			\
	size_t _len;							\
	int _unsafe;							\
									\
	_len = sizeof(_unsafe);						\
	if (sysctlbyname("vfs.aio.enable_unsafe", &_unsafe, &_len, NULL,\
	    0) < 0) {							\
		if (errno != ENOENT) {					\
			printf("Failed to read vfs.aio.enable_unsafe: %s\n",\
			    strerror(errno));				\
			_exit(1);					\
		}							\
	} else if (_unsafe == 0) {					\
		printf("Unsafe AIO is disabled");			\
		_exit(_exit_code);					\
	}								\
} while (0)

#endif /* !_AIO_TEST_LOCAL_H_ */
