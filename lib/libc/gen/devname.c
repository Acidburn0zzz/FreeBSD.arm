/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)devname.c	8.2 (Berkeley) 4/29/95";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/libc/gen/devname.c 298226 2016-04-18 21:05:15Z avos $");

#include <sys/param.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *
devname_r(dev_t dev, mode_t type, char *buf, int len)
{
	int i;
	size_t j;

	if (dev == NODEV || !(S_ISCHR(type) || S_ISBLK(dev))) {
		strlcpy(buf, "#NODEV", len);
		return (buf);
	}

	if (S_ISCHR(type)) {
		j = len;
		i = sysctlbyname("kern.devname", buf, &j, &dev, sizeof (dev));
		if (i == 0)
			return (buf);
	}

	/* Finally just format it */
	snprintf(buf, len, "#%c:%#jx",
	    S_ISCHR(type) ? 'C' : 'B', (uintmax_t)dev);
	return (buf);
}

char *
devname(dev_t dev, mode_t type)
{
	static char buf[SPECNAMELEN + 1];

	return (devname_r(dev, type, buf, sizeof(buf)));
}
