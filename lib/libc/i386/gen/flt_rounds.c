/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/libc/i386/gen/flt_rounds.c 132383 2004-07-19 08:17:25Z das $");

#include <float.h>

static const int map[] = {
	1,	/* round to nearest */
	3,	/* round to zero */
	2,	/* round to negative infinity */
	0	/* round to positive infinity */
};

int
__flt_rounds(void)
{
	int x;

	__asm("fnstcw %0" : "=m" (x));
        return (map[(x >> 10) & 0x03]);
}
