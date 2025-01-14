/* @(#)e_lgamma.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/msun/src/e_lgammal.c 271651 2014-09-15 23:21:57Z kargl $");

#include "math.h"
#include "math_private.h"

extern int signgam;

long double
lgammal(long double x)
{
	return lgammal_r(x,&signgam);
}
