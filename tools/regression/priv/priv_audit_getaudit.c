/*-
 * Copyright (c) 2007 Robert M. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: head/tools/regression/priv/priv_audit_getaudit.c 172106 2007-09-09 23:08:39Z rwatson $
 */

/*
 * Confirm that privilege is required to query process audit state.
 */

#include <sys/types.h>

#include <bsm/audit.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>

#include "main.h"

int
priv_audit_getaudit_setup(int asroot, int injail, struct test *test)
{

	/*
	 * XXXRW: It would be nice if we checked for audit being configured
	 * here.
	 */
	return (0);
}

void
priv_audit_getaudit(int asroot, int injail, struct test *test)
{
	auditinfo_t ai;
	int error;

	error = getaudit(&ai);
	if (asroot && injail)
		expect("priv_audit_getaudit(asroot, injail)", error, -1,
		    ENOSYS);
	if (asroot && !injail)
		expect("priv_audit_getaudit(asroot, !injail)", error, 0, 0);
	if (!asroot && injail)
		expect("priv_audit_getaudit(!asroot, injail)", error, -1,
		    ENOSYS);
	if (!asroot && !injail)
		expect("priv_audit_getaudit(!asroot, !injail)", error, -1,
		    EPERM);
}

void
priv_audit_getaudit_addr(int asroot, int injail, struct test *test)
{
	auditinfo_addr_t aia;
	int error;

	error = getaudit_addr(&aia, sizeof(aia));
	if (asroot && injail)
		expect("priv_audit_getaudit_addr(asroot, injail)", error, -1,
		    ENOSYS);
	if (asroot && !injail)
		expect("priv_audit_getaudit_addr(asroot, !injail)", error, 0,
		    0);
	if (!asroot && injail)
		expect("priv_audit_getaudit_addr(!asroot, injail)", error,
		    -1, ENOSYS);
	if (!asroot && !injail)
		expect("priv_audit_getaudit_addr(!asroot, !injail)", error,
		    -1, EPERM);
}

void
priv_audit_getaudit_cleanup(int asroot, int injail, struct test *test)
{

}
