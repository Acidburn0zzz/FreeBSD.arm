/*-
 * Copyright (c) 1999 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/kern/subr_eventhandler.c 205345 2010-03-19 19:51:03Z bz $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>

static MALLOC_DEFINE(M_EVENTHANDLER, "eventhandler", "Event handler records");

/* List of 'slow' lists */
static TAILQ_HEAD(, eventhandler_list)	eventhandler_lists;
static int				eventhandler_lists_initted = 0;
static struct mtx			eventhandler_mutex;

struct eventhandler_entry_generic 
{
    struct eventhandler_entry	ee;
    void			(* func)(void);
};

static struct eventhandler_list *_eventhandler_find_list(const char *name);

/*
 * Initialize the eventhandler mutex and list.
 */
static void
eventhandler_init(void *dummy __unused)
{
    TAILQ_INIT(&eventhandler_lists);
    mtx_init(&eventhandler_mutex, "eventhandler", NULL, MTX_DEF);
    atomic_store_rel_int(&eventhandler_lists_initted, 1);
}
SYSINIT(eventhandlers, SI_SUB_EVENTHANDLER, SI_ORDER_FIRST, eventhandler_init,
    NULL);

/* 
 * Insertion is O(n) due to the priority scan, but optimises to O(1)
 * if all priorities are identical.
 */
static eventhandler_tag
eventhandler_register_internal(struct eventhandler_list *list,
    const char *name, eventhandler_tag epn)
{
    struct eventhandler_list		*new_list;
    struct eventhandler_entry		*ep;
    
    KASSERT(eventhandler_lists_initted, ("eventhandler registered too early"));
    KASSERT(epn != NULL, ("%s: cannot register NULL event", __func__));

    /* lock the eventhandler lists */
    mtx_lock(&eventhandler_mutex);

    /* Do we need to find/create the (slow) list? */
    if (list == NULL) {
	/* look for a matching, existing list */
	list = _eventhandler_find_list(name);

	/* Do we need to create the list? */
	if (list == NULL) {
	    mtx_unlock(&eventhandler_mutex);

	    new_list = malloc(sizeof(struct eventhandler_list) +
		strlen(name) + 1, M_EVENTHANDLER, M_WAITOK);

	    /* If someone else created it already, then use that one. */
	    mtx_lock(&eventhandler_mutex);
	    list = _eventhandler_find_list(name);
	    if (list != NULL) {
		free(new_list, M_EVENTHANDLER);
	    } else {
		CTR2(KTR_EVH, "%s: creating list \"%s\"", __func__, name);
		list = new_list;
		list->el_flags = 0;
		list->el_runcount = 0;
		bzero(&list->el_lock, sizeof(list->el_lock));
		list->el_name = (char *)list + sizeof(struct eventhandler_list);
		strcpy(list->el_name, name);
		TAILQ_INSERT_HEAD(&eventhandler_lists, list, el_link);
	    }
	}
    }
    if (!(list->el_flags & EHL_INITTED)) {
	TAILQ_INIT(&list->el_entries);
	mtx_init(&list->el_lock, name, "eventhandler list", MTX_DEF);
	atomic_store_rel_int(&list->el_flags, EHL_INITTED);
    }
    mtx_unlock(&eventhandler_mutex);

    KASSERT(epn->ee_priority != EHE_DEAD_PRIORITY,
	("%s: handler for %s registered with dead priority", __func__, name));

    /* sort it into the list */
    CTR4(KTR_EVH, "%s: adding item %p (function %p) to \"%s\"", __func__, epn,
	((struct eventhandler_entry_generic *)epn)->func, name);
    EHL_LOCK(list);
    TAILQ_FOREACH(ep, &list->el_entries, ee_link) {
	if (ep->ee_priority != EHE_DEAD_PRIORITY &&
	    epn->ee_priority < ep->ee_priority) {
	    TAILQ_INSERT_BEFORE(ep, epn, ee_link);
	    break;
	}
    }
    if (ep == NULL)
	TAILQ_INSERT_TAIL(&list->el_entries, epn, ee_link);
    EHL_UNLOCK(list);
    return(epn);
}

eventhandler_tag
eventhandler_register(struct eventhandler_list *list, const char *name, 
		      void *func, void *arg, int priority)
{
    struct eventhandler_entry_generic	*eg;
    
    /* allocate an entry for this handler, populate it */
    eg = malloc(sizeof(struct eventhandler_entry_generic), M_EVENTHANDLER,
	M_WAITOK | M_ZERO);
    eg->func = func;
    eg->ee.ee_arg = arg;
    eg->ee.ee_priority = priority;

    return (eventhandler_register_internal(list, name, &eg->ee));
}

#ifdef VIMAGE
struct eventhandler_entry_generic_vimage
{
    struct eventhandler_entry		ee;
    vimage_iterator_func_t		func;		/* Vimage iterator function. */
    struct eventhandler_entry_vimage	v_ee;		/* Original func, arg. */
};

eventhandler_tag
vimage_eventhandler_register(struct eventhandler_list *list, const char *name, 
    void *func, void *arg, int priority, vimage_iterator_func_t iterfunc)
{
    struct eventhandler_entry_generic_vimage	*eg;
    
    /* allocate an entry for this handler, populate it */
    eg = malloc(sizeof(struct eventhandler_entry_generic_vimage),
	M_EVENTHANDLER, M_WAITOK | M_ZERO);
    eg->func = iterfunc;
    eg->v_ee.func = func;
    eg->v_ee.ee_arg = arg;
    eg->ee.ee_arg = &eg->v_ee;
    eg->ee.ee_priority = priority;

    return (eventhandler_register_internal(list, name, &eg->ee));
}
#endif

void
eventhandler_deregister(struct eventhandler_list *list, eventhandler_tag tag)
{
    struct eventhandler_entry	*ep = tag;

    EHL_LOCK_ASSERT(list, MA_OWNED);
    if (ep != NULL) {
	/* remove just this entry */
	if (list->el_runcount == 0) {
	    CTR3(KTR_EVH, "%s: removing item %p from \"%s\"", __func__, ep,
		list->el_name);
	    TAILQ_REMOVE(&list->el_entries, ep, ee_link);
	    free(ep, M_EVENTHANDLER);
	} else {
	    CTR3(KTR_EVH, "%s: marking item %p from \"%s\" as dead", __func__,
		ep, list->el_name);
	    ep->ee_priority = EHE_DEAD_PRIORITY;
	}
    } else {
	/* remove entire list */
	if (list->el_runcount == 0) {
	    CTR2(KTR_EVH, "%s: removing all items from \"%s\"", __func__,
		list->el_name);
	    while (!TAILQ_EMPTY(&list->el_entries)) {
		ep = TAILQ_FIRST(&list->el_entries);
		TAILQ_REMOVE(&list->el_entries, ep, ee_link);
		free(ep, M_EVENTHANDLER);
	    }
	} else {
	    CTR2(KTR_EVH, "%s: marking all items from \"%s\" as dead",
		__func__, list->el_name);
	    TAILQ_FOREACH(ep, &list->el_entries, ee_link)
		ep->ee_priority = EHE_DEAD_PRIORITY;
	}
    }
    while (list->el_runcount > 0)
	    mtx_sleep(list, &list->el_lock, 0, "evhrm", 0);
    EHL_UNLOCK(list);
}

/*
 * Internal version for use when eventhandler list is already locked.
 */
static struct eventhandler_list *
_eventhandler_find_list(const char *name)
{
    struct eventhandler_list	*list;

    mtx_assert(&eventhandler_mutex, MA_OWNED);
    TAILQ_FOREACH(list, &eventhandler_lists, el_link) {
	if (!strcmp(name, list->el_name))
	    break;
    }
    return (list);
}

/*
 * Lookup a "slow" list by name.  Returns with the list locked.
 */
struct eventhandler_list *
eventhandler_find_list(const char *name)
{
    struct eventhandler_list	*list;

    if (!eventhandler_lists_initted)
	return(NULL);
    
    /* scan looking for the requested list */
    mtx_lock(&eventhandler_mutex);
    list = _eventhandler_find_list(name);
    if (list != NULL)
	EHL_LOCK(list);
    mtx_unlock(&eventhandler_mutex);
    
    return(list);
}

/*
 * Prune "dead" entries from an eventhandler list.
 */
void
eventhandler_prune_list(struct eventhandler_list *list)
{
    struct eventhandler_entry *ep, *en;
    int pruned = 0;

    CTR2(KTR_EVH, "%s: pruning list \"%s\"", __func__, list->el_name);
    EHL_LOCK_ASSERT(list, MA_OWNED);
    TAILQ_FOREACH_SAFE(ep, &list->el_entries, ee_link, en) {
	if (ep->ee_priority == EHE_DEAD_PRIORITY) {
	    TAILQ_REMOVE(&list->el_entries, ep, ee_link);
	    free(ep, M_EVENTHANDLER);
	    pruned++;
	}
    }
    if (pruned > 0)
	    wakeup(list);
}
