/*-
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend.c#3 $
 */
/*
 * CTL backend driver registration routines
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/cam/ctl/ctl_backend.c 288211 2015-09-25 07:27:23Z mav $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_debug.h>

extern struct ctl_softc *control_softc;

int
ctl_backend_register(struct ctl_backend_driver *be)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_backend_driver *be_tmp;

	mtx_lock(&softc->ctl_lock);
	/*
	 * Sanity check, make sure this isn't a duplicate registration.
	 */
	STAILQ_FOREACH(be_tmp, &softc->be_list, links) {
		if (strcmp(be_tmp->name, be->name) == 0) {
			mtx_unlock(&softc->ctl_lock);
			return (-1);
		}
	}
	mtx_unlock(&softc->ctl_lock);

	/*
	 * Call the backend's initialization routine.
	 */
	be->init();

	mtx_lock(&softc->ctl_lock);
	
	STAILQ_INSERT_TAIL(&softc->be_list, be, links);

	softc->num_backends++;

	/*
	 * Don't want to increment the usage count for internal consumers,
	 * we won't be able to unload otherwise.
	 */
	/* XXX KDM find a substitute for this? */
#if 0
	if ((be->flags & CTL_BE_FLAG_INTERNAL) == 0)
		MOD_INC_USE_COUNT;
#endif

#ifdef CS_BE_CONFIG_MOVE_DONE_IS_NOT_USED
	be->config_move_done = ctl_config_move_done;
#endif
	/* XXX KDM fix this! */
	be->num_luns = 0;
#if 0
	atomic_set(&be->num_luns, 0);
#endif

	mtx_unlock(&softc->ctl_lock);

	return (0);
}

int
ctl_backend_deregister(struct ctl_backend_driver *be)
{
	struct ctl_softc *softc = control_softc;

	mtx_lock(&softc->ctl_lock);

#if 0
	if (atomic_read(&be->num_luns) != 0) {
#endif
	/* XXX KDM fix this! */
	if (be->num_luns != 0) {
		mtx_unlock(&softc->ctl_lock);
		return (-1);
	}

	STAILQ_REMOVE(&softc->be_list, be, ctl_backend_driver, links);

	softc->num_backends--;

	/* XXX KDM find a substitute for this? */
#if 0
	if ((be->flags & CTL_BE_FLAG_INTERNAL) == 0)
		MOD_DEC_USE_COUNT;
#endif

	mtx_unlock(&softc->ctl_lock);

	return (0);
}

struct ctl_backend_driver *
ctl_backend_find(char *backend_name)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_backend_driver *be_tmp;

	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(be_tmp, &softc->be_list, links) {
		if (strcmp(be_tmp->name, backend_name) == 0) {
			mtx_unlock(&softc->ctl_lock);
			return (be_tmp);
		}
	}
	mtx_unlock(&softc->ctl_lock);

	return (NULL);
}

void
ctl_init_opts(ctl_options_t *opts, int num_args, struct ctl_be_arg *args)
{
	struct ctl_option *opt;
	int i;

	STAILQ_INIT(opts);
	for (i = 0; i < num_args; i++) {
		if ((args[i].flags & CTL_BEARG_RD) == 0)
			continue;
		if ((args[i].flags & CTL_BEARG_ASCII) == 0)
			continue;
		opt = malloc(sizeof(*opt), M_CTL, M_WAITOK);
		opt->name = strdup(args[i].kname, M_CTL);
		opt->value = strdup(args[i].kvalue, M_CTL);
		STAILQ_INSERT_TAIL(opts, opt, links);
	}
}

void
ctl_update_opts(ctl_options_t *opts, int num_args, struct ctl_be_arg *args)
{
	struct ctl_option *opt;
	int i;

	for (i = 0; i < num_args; i++) {
		if ((args[i].flags & CTL_BEARG_RD) == 0)
			continue;
		if ((args[i].flags & CTL_BEARG_ASCII) == 0)
			continue;
		STAILQ_FOREACH(opt, opts, links) {
			if (strcmp(opt->name, args[i].kname) == 0)
				break;
		}
		if (args[i].kvalue != NULL &&
		    ((char *)args[i].kvalue)[0] != 0) {
			if (opt) {
				free(opt->value, M_CTL);
				opt->value = strdup(args[i].kvalue, M_CTL);
			} else {
				opt = malloc(sizeof(*opt), M_CTL, M_WAITOK);
				opt->name = strdup(args[i].kname, M_CTL);
				opt->value = strdup(args[i].kvalue, M_CTL);
				STAILQ_INSERT_TAIL(opts, opt, links);
			}
		} else if (opt) {
			STAILQ_REMOVE(opts, opt, ctl_option, links);
			free(opt->name, M_CTL);
			free(opt->value, M_CTL);
			free(opt, M_CTL);
		}
	}
}

void
ctl_free_opts(ctl_options_t *opts)
{
	struct ctl_option *opt;

	while ((opt = STAILQ_FIRST(opts)) != NULL) {
		STAILQ_REMOVE_HEAD(opts, links);
		free(opt->name, M_CTL);
		free(opt->value, M_CTL);
		free(opt, M_CTL);
	}
}

char *
ctl_get_opt(ctl_options_t *opts, const char *name)
{
	struct ctl_option *opt;

	STAILQ_FOREACH(opt, opts, links) {
		if (strcmp(opt->name, name) == 0) {
			return (opt->value);
		}
	}
	return (NULL);
}
