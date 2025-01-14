/*	$FreeBSD: head/usr.bin/bc/extern.h 291155 2015-11-22 02:43:14Z pfg $						*/
/*      $OpenBSD: extern.h,v 1.12 2014/04/17 19:07:14 otto Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdbool.h>
#include <stdio.h>

struct lvalue {
	ssize_t load;
	ssize_t store;
};

int		yylex(void);
void		yyerror(const char *);
void		fatal(const char *);
void		abort_line(int);
struct termios;
int		gettty(struct termios *);
void		tstpcont(int);
unsigned char	bc_eof(EditLine *, int);

extern int	lineno;
extern int	fileindex;
extern int	sargc;
extern const char	**sargv;
extern const char	*filename;
extern bool	 interactive;
extern EditLine	*el;
extern History	*hist;
extern HistEvent he;
extern char	*cmdexpr;
extern struct termios ttysaved;
