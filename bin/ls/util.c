/*-
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)util.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/bin/ls/util.c 284198 2015-06-10 01:27:38Z marcel $");

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <libxo/xo.h>

#include "ls.h"
#include "extern.h"

int
prn_normal(const char *field, const char *s)
{
	char fmt[_POSIX2_LINE_MAX];

	snprintf(fmt, sizeof(fmt), "{:%s/%%hs}", field);
	return xo_emit(fmt, s);
#if 0
	mbstate_t mbs;
	wchar_t wc;
	int i, n;
	size_t clen;

	memset(&mbs, 0, sizeof(mbs));
	n = 0;
	while ((clen = mbrtowc(&wc, s, MB_LEN_MAX, &mbs)) != 0) {
		if (clen == (size_t)-2) {
			n += printf("%s", s);
			break;
		}
		if (clen == (size_t)-1) {
			memset(&mbs, 0, sizeof(mbs));
			putchar((unsigned char)*s);
			s++;
			n++;
			continue;
		}
		for (i = 0; i < (int)clen; i++)
			putchar((unsigned char)s[i]);
		s += clen;
		if (iswprint(wc))
			n += wcwidth(wc);
	}
	return (n);
#endif
}

char *
get_printable(const char *s)
{
	mbstate_t mbs;
	wchar_t wc;
	int i, n;
	size_t clen;
	int slen = strlen(s);
	char *buf = alloca(slen + 1), *bp = buf;

	memset(&mbs, 0, sizeof(mbs));
	n = 0;
	while ((clen = mbrtowc(&wc, s, MB_LEN_MAX, &mbs)) != 0) {
		if (clen == (size_t)-1) {
			*bp++ = '?';
			s++;
			n++;
			memset(&mbs, 0, sizeof(mbs));
			continue;
		}
		if (clen == (size_t)-2) {
			*bp++ = '?';
			n++;
			break;
		}
		if (!iswprint(wc)) {
			*bp++ = '?';
			s += clen;
			n++;
			continue;
		}
		for (i = 0; i < (int)clen; i++)
			*bp++ = (unsigned char)s[i];
		s += clen;
		n += wcwidth(wc);
	}
	*bp = '\0';
	return strdup(buf);
}

/*
 * The fts system makes it difficult to replace fts_name with a different-
 * sized string, so we just calculate the real length here and do the
 * conversion in prn_octal()
 *
 * XXX when using f_octal_escape (-b) rather than f_octal (-B), the
 * length computed by len_octal may be too big. I just can't be buggered
 * to fix this as an efficient fix would involve a lookup table. Same goes
 * for the rather inelegant code in prn_octal.
 *
 *						DES 1998/04/23
 */

size_t
len_octal(const char *s, int len)
{
	mbstate_t mbs;
	wchar_t wc;
	size_t clen, r;

	memset(&mbs, 0, sizeof(mbs));
	r = 0;
	while (len != 0 && (clen = mbrtowc(&wc, s, len, &mbs)) != 0) {
		if (clen == (size_t)-1) {
			r += 4;
			s++;
			len--;
			memset(&mbs, 0, sizeof(mbs));
			continue;
		}
		if (clen == (size_t)-2) {
			r += 4 * len;
			break;
		}
		if (iswprint(wc))
			r++;
		else
			r += 4 * clen;
		s += clen;
	}
	return (r);
}

char *
get_octal(const char *s)
{
	static const char esc[] = "\\\\\"\"\aa\bb\ff\nn\rr\tt\vv";
	const char *p;
	mbstate_t mbs;
	wchar_t wc;
	size_t clen;
	unsigned char ch;
	int goodchar, i, len, prtlen;
	int slen = strlen(s);
	char *buf = alloca(slen * 4 + 1), *bp = buf;

	memset(&mbs, 0, sizeof(mbs));
	len = 0;
	while ((clen = mbrtowc(&wc, s, MB_LEN_MAX, &mbs)) != 0) {
		goodchar = clen != (size_t)-1 && clen != (size_t)-2;
		if (goodchar && iswprint(wc) && wc != L'\"' && wc != L'\\') {
			for (i = 0; i < (int)clen; i++)
				*bp++ = (unsigned char)s[i];
			len += wcwidth(wc);
		} else if (goodchar && f_octal_escape &&
#if WCHAR_MIN < 0
                    wc >= 0 &&
#endif
		    wc <= (wchar_t)UCHAR_MAX &&
		    (p = strchr(esc, (char)wc)) != NULL) {
			*bp ++ = '\\';
			*bp++ = p[1];
			len += 2;
		} else {
			if (goodchar)
				prtlen = clen;
			else if (clen == (size_t)-1)
				prtlen = 1;
			else
				prtlen = strlen(s);
			for (i = 0; i < prtlen; i++) {
				ch = (unsigned char)s[i];
				*bp++ = '\\';
				*bp++ = '0' + (ch >> 6);
				*bp++ = '0' + ((ch >> 3) & 7);
				*bp++ = '0' + (ch & 7);
				len += 4;
			}
		}
		if (clen == (size_t)-2)
			break;
		if (clen == (size_t)-1) {
			memset(&mbs, 0, sizeof(mbs));
			s++;
		} else
			s += clen;
	}

	*bp = '\0';
	return strdup(buf);
}

void
usage(void)
{
	xo_error(
#ifdef COLORLS
	"usage: ls [-ABCFGHILPRSTUWZabcdfghiklmnopqrstuwxy1,] [-D format]"
#else
	"usage: ls [-ABCFHILPRSTUWZabcdfghiklmnopqrstuwxy1,] [-D format]"
#endif
		      " [file ...]\n");
	exit(1);
}
