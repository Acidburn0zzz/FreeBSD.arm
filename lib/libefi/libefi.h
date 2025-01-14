/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
 *
 * $FreeBSD: head/lib/libefi/libefi.h 203181 2010-01-30 04:24:03Z marcel $
 */

#ifndef _LIBEFI_H_
#define	_LIBEFI_H_

#include <sys/types.h>
#include <sys/uuid.h>
#include <stddef.h>

/* Attributes. */
#define	EFI_ATTR_NV	0x0001	/* Variable stored in NVRAM. */
#define	EFI_ATTR_BS	0x0002	/* Boot services accessable. */
#define	EFI_ATTR_RT	0x0004	/* Runtime accessable. */
#define	EFI_ATTR_HR	0x0008	/* Hardware error record. */
#define	EFI_ATTR_WR	0x0010	/* Authenticated write access. */

/* Vendor for architecturally defined variables. */
#define	EFI_GLOBAL_VARIABLE	\
	{0x8be4df61,0x93ca,0x11d2,0xaa,0x0d,{0x00,0xe0,0x98,0x03,0x2b,0x8c}}

/* Vendor for FreeBSD-specific variables. */
#define	EFI_FREEBSD_VARIABLE	\
	{0x13c32014,0x0c9c,0x11df,0xa2,0x38,{0x00,0x17,0xa4,0xab,0x91,0x2d}}

__BEGIN_DECLS
int	efi_getvar	(char *, uuid_t *, uint32_t *, size_t *, void *);
int	efi_nextvarname	(size_t *, char *, uuid_t *);
int	efi_setvar	(char *, uuid_t *, uint32_t, size_t, void *);
__END_DECLS

#endif /* _LIBEFI_H_ */
