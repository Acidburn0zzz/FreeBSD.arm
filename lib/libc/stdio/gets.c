/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)gets.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/libc/stdio/gets.c 268928 2014-07-20 20:29:28Z pfg $");

#include "namespace.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/cdefs.h>
#include "un-namespace.h"
#include "libc_private.h"
#include "local.h"

__warn_references(gets, "warning: this program uses gets(), which is unsafe.");

char *
gets(char *buf)
{
	int c;
	char *s;
	static int warned;
	static const char w[] =
	    "warning: this program uses gets(), which is unsafe.\n";

	FLOCKFILE(stdin);
	ORIENT(stdin, -1);
	if (!warned) {
		(void) _write(STDERR_FILENO, w, sizeof(w) - 1);
		warned = 1;
	}
	for (s = buf; (c = __sgetc(stdin)) != '\n';)
		if (c == EOF)
			if (s == buf) {
				FUNLOCKFILE(stdin);
				return (NULL);
			} else
				break;
		else
			*s++ = c;
	*s = 0;
	FUNLOCKFILE(stdin);
	return (buf);
}
