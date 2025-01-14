/*-
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Poul-Henning Kamp of the FreeBSD Project.
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
 *
 *	@(#)vfs_cache.c	8.5 (Berkeley) 3/22/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/kern/vfs_cache.c 298819 2016-04-29 22:15:33Z pfg $");

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sdt.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/uma.h>

SDT_PROVIDER_DECLARE(vfs);
SDT_PROBE_DEFINE3(vfs, namecache, enter, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, enter_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, entry, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, hit, "struct vnode *",
    "char *", "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, miss, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, return, "int",
    "struct vnode *", "char *");
SDT_PROBE_DEFINE3(vfs, namecache, lookup, hit, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, hit__negative,
    "struct vnode *", "char *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, miss, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE1(vfs, namecache, purge, done, "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, purge_negative, done, "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, purgevfs, done, "struct mount *");
SDT_PROBE_DEFINE3(vfs, namecache, zap, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, zap_negative, done, "struct vnode *",
    "char *");

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei.
 */

struct	namecache {
	LIST_ENTRY(namecache) nc_hash;	/* hash chain */
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	struct	vnode *nc_vp;		/* vnode the name refers to */
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	char	nc_name[0];		/* segment name + nul */
};

/*
 * struct namecache_ts repeats struct namecache layout up to the
 * nc_nlen member.
 * struct namecache_ts is used in place of struct namecache when time(s) need
 * to be stored.  The nc_dotdottime field is used when a cache entry is mapping
 * both a non-dotdot directory name plus dotdot for the directory's
 * parent.
 */
struct	namecache_ts {
	LIST_ENTRY(namecache) nc_hash;	/* hash chain */
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	struct	vnode *nc_vp;		/* vnode the name refers to */
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	struct	timespec nc_time;	/* timespec provided by fs */
	struct	timespec nc_dotdottime;	/* dotdot timespec provided by fs */
	int	nc_ticks;		/* ticks value when entry was added */
	char	nc_name[0];		/* segment name + nul */
};

/*
 * Flags in namecache.nc_flag
 */
#define NCF_WHITE	0x01
#define NCF_ISDOTDOT	0x02
#define	NCF_TS		0x04
#define	NCF_DTS		0x08

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * If it is a "negative" entry, (i.e. for a name that is known NOT to
 * exist) the vnode pointer will be NULL.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 */

/*
 * Structures associated with name caching.
 */
#define NCHHASH(hash) \
	(&nchashtbl[(hash) & nchash])
static LIST_HEAD(nchashhead, namecache) *nchashtbl;	/* Hash Table */
static TAILQ_HEAD(, namecache) ncneg;	/* Hash Table */
static u_long	nchash;			/* size of hash table */
SYSCTL_ULONG(_debug, OID_AUTO, nchash, CTLFLAG_RD, &nchash, 0,
    "Size of namecache hash table");
static u_long	ncnegfactor = 16;	/* ratio of negative entries */
SYSCTL_ULONG(_vfs, OID_AUTO, ncnegfactor, CTLFLAG_RW, &ncnegfactor, 0,
    "Ratio of negative namecache entries");
static u_long	numneg;			/* number of negative entries allocated */
SYSCTL_ULONG(_debug, OID_AUTO, numneg, CTLFLAG_RD, &numneg, 0,
    "Number of negative entries in namecache");
static u_long	numcache;		/* number of cache entries allocated */
SYSCTL_ULONG(_debug, OID_AUTO, numcache, CTLFLAG_RD, &numcache, 0,
    "Number of namecache entries");
static u_long	numcachehv;		/* number of cache entries with vnodes held */
SYSCTL_ULONG(_debug, OID_AUTO, numcachehv, CTLFLAG_RD, &numcachehv, 0,
    "Number of namecache entries with vnodes held");
u_int	ncsizefactor = 2;
SYSCTL_UINT(_vfs, OID_AUTO, ncsizefactor, CTLFLAG_RW, &ncsizefactor, 0,
    "Size factor for namecache");

struct nchstats	nchstats;		/* cache effectiveness statistics */

static struct rwlock cache_lock;
RW_SYSINIT(vfscache, &cache_lock, "Name Cache");

#define	CACHE_UPGRADE_LOCK()	rw_try_upgrade(&cache_lock)
#define	CACHE_RLOCK()		rw_rlock(&cache_lock)
#define	CACHE_RUNLOCK()		rw_runlock(&cache_lock)
#define	CACHE_WLOCK()		rw_wlock(&cache_lock)
#define	CACHE_WUNLOCK()		rw_wunlock(&cache_lock)

/*
 * UMA zones for the VFS cache.
 *
 * The small cache is used for entries with short names, which are the
 * most common.  The large cache is used for entries which are too big to
 * fit in the small cache.
 */
static uma_zone_t cache_zone_small;
static uma_zone_t cache_zone_small_ts;
static uma_zone_t cache_zone_large;
static uma_zone_t cache_zone_large_ts;

#define	CACHE_PATH_CUTOFF	35

static struct namecache *
cache_alloc(int len, int ts)
{

	if (len > CACHE_PATH_CUTOFF) {
		if (ts)
			return (uma_zalloc(cache_zone_large_ts, M_WAITOK));
		else
			return (uma_zalloc(cache_zone_large, M_WAITOK));
	}
	if (ts)
		return (uma_zalloc(cache_zone_small_ts, M_WAITOK));
	else
		return (uma_zalloc(cache_zone_small, M_WAITOK));
}

static void
cache_free(struct namecache *ncp)
{
	int ts;

	if (ncp == NULL)
		return;
	ts = ncp->nc_flag & NCF_TS;
	if (ncp->nc_nlen <= CACHE_PATH_CUTOFF) {
		if (ts)
			uma_zfree(cache_zone_small_ts, ncp);
		else
			uma_zfree(cache_zone_small, ncp);
	} else if (ts)
		uma_zfree(cache_zone_large_ts, ncp);
	else
		uma_zfree(cache_zone_large, ncp);
}

static char *
nc_get_name(struct namecache *ncp)
{
	struct namecache_ts *ncp_ts;

	if ((ncp->nc_flag & NCF_TS) == 0)
		return (ncp->nc_name);
	ncp_ts = (struct namecache_ts *)ncp;
	return (ncp_ts->nc_name);
}

static void
cache_out_ts(struct namecache *ncp, struct timespec *tsp, int *ticksp)
{

	KASSERT((ncp->nc_flag & NCF_TS) != 0 ||
	    (tsp == NULL && ticksp == NULL),
	    ("No NCF_TS"));

	if (tsp != NULL)
		*tsp = ((struct namecache_ts *)ncp)->nc_time;
	if (ticksp != NULL)
		*ticksp = ((struct namecache_ts *)ncp)->nc_ticks;
}

static int	doingcache = 1;		/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0,
    "VFS namecache enabled");

/* Export size information to userland */
SYSCTL_INT(_debug_sizeof, OID_AUTO, namecache, CTLFLAG_RD, SYSCTL_NULL_INT_PTR,
    sizeof(struct namecache), "sizeof(struct namecache)");

/*
 * The new name cache statistics
 */
static SYSCTL_NODE(_vfs, OID_AUTO, cache, CTLFLAG_RW, 0,
    "Name cache statistics");
#define STATNODE_ULONG(name, descr)	\
	SYSCTL_ULONG(_vfs_cache, OID_AUTO, name, CTLFLAG_RD, &name, 0, descr);
#define STATNODE_COUNTER(name, descr)	\
	static counter_u64_t name;	\
	SYSCTL_COUNTER_U64(_vfs_cache, OID_AUTO, name, CTLFLAG_RD, &name, descr);
STATNODE_ULONG(numneg, "Number of negative cache entries");
STATNODE_ULONG(numcache, "Number of cache entries");
STATNODE_COUNTER(numcalls, "Number of cache lookups");
STATNODE_COUNTER(dothits, "Number of '.' hits");
STATNODE_COUNTER(dotdothits, "Number of '..' hits");
STATNODE_COUNTER(numchecks, "Number of checks in lookup");
STATNODE_COUNTER(nummiss, "Number of cache misses");
STATNODE_COUNTER(nummisszap, "Number of cache misses we do not want to cache");
STATNODE_COUNTER(numposzaps,
    "Number of cache hits (positive) we do not want to cache");
STATNODE_COUNTER(numposhits, "Number of cache hits (positive)");
STATNODE_COUNTER(numnegzaps,
    "Number of cache hits (negative) we do not want to cache");
STATNODE_COUNTER(numneghits, "Number of cache hits (negative)");
/* These count for kern___getcwd(), too. */
STATNODE_COUNTER(numfullpathcalls, "Number of fullpath search calls");
STATNODE_COUNTER(numfullpathfail1, "Number of fullpath search errors (ENOTDIR)");
STATNODE_COUNTER(numfullpathfail2,
    "Number of fullpath search errors (VOP_VPTOCNP failures)");
STATNODE_COUNTER(numfullpathfail4, "Number of fullpath search errors (ENOMEM)");
STATNODE_COUNTER(numfullpathfound, "Number of successful fullpath calls");
static long numupgrades; STATNODE_ULONG(numupgrades,
    "Number of updates of the cache after lookup (write lock + retry)");

static void cache_zap(struct namecache *ncp);
static int vn_vptocnp_locked(struct vnode **vp, struct ucred *cred, char *buf,
    u_int *buflen);
static int vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen);

static MALLOC_DEFINE(M_VFSCACHE, "vfscache", "VFS name cache entries");

static uint32_t
cache_get_hash(char *name, u_char len, struct vnode *dvp)
{
	uint32_t hash;

	hash = fnv_32_buf(name, len, FNV1_32_INIT);
	hash = fnv_32_buf(&dvp, sizeof(dvp), hash);
	return (hash);
}

static int
sysctl_nchstats(SYSCTL_HANDLER_ARGS)
{
	struct nchstats snap;

	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, sizeof(snap)));

	snap = nchstats;
	snap.ncs_goodhits = counter_u64_fetch(numposhits);
	snap.ncs_neghits = counter_u64_fetch(numneghits);
	snap.ncs_badhits = counter_u64_fetch(numposzaps) +
	    counter_u64_fetch(numnegzaps);
	snap.ncs_miss = counter_u64_fetch(nummisszap) +
	    counter_u64_fetch(nummiss);

	return (SYSCTL_OUT(req, &snap, sizeof(snap)));
}
SYSCTL_PROC(_vfs_cache, OID_AUTO, nchstats, CTLTYPE_OPAQUE | CTLFLAG_RD |
    CTLFLAG_MPSAFE, 0, 0, sysctl_nchstats, "LU",
    "VFS cache effectiveness statistics");

#ifdef DIAGNOSTIC
/*
 * Grab an atomic snapshot of the name cache hash chain lengths
 */
static SYSCTL_NODE(_debug, OID_AUTO, hashstat, CTLFLAG_RW, NULL,
    "hash table stats");

static int
sysctl_debug_hashstat_rawnchash(SYSCTL_HANDLER_ARGS)
{
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int i, error, n_nchash, *cntbuf;

retry:
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	if (req->oldptr == NULL)
		return SYSCTL_OUT(req, 0, n_nchash * sizeof(int));
	cntbuf = malloc(n_nchash * sizeof(int), M_TEMP, M_ZERO | M_WAITOK);
	CACHE_RLOCK();
	if (n_nchash != nchash + 1) {
		CACHE_RUNLOCK();
		free(cntbuf, M_TEMP);
		goto retry;
	}
	/* Scan hash tables counting entries */
	for (ncpp = nchashtbl, i = 0; i < n_nchash; ncpp++, i++)
		LIST_FOREACH(ncp, ncpp, nc_hash)
			cntbuf[i]++;
	CACHE_RUNLOCK();
	for (error = 0, i = 0; i < n_nchash; i++)
		if ((error = SYSCTL_OUT(req, &cntbuf[i], sizeof(int))) != 0)
			break;
	free(cntbuf, M_TEMP);
	return (error);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, rawnchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_rawnchash, "S,int",
    "nchash chain lengths");

static int
sysctl_debug_hashstat_nchash(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int n_nchash;
	int count, maxlength, used, pct;

	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, 4 * sizeof(int));

	CACHE_RLOCK();
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	used = 0;
	maxlength = 0;

	/* Scan hash tables for applicable entries */
	for (ncpp = nchashtbl; n_nchash > 0; n_nchash--, ncpp++) {
		count = 0;
		LIST_FOREACH(ncp, ncpp, nc_hash) {
			count++;
		}
		if (count)
			used++;
		if (maxlength < count)
			maxlength = count;
	}
	n_nchash = nchash + 1;
	CACHE_RUNLOCK();
	pct = (used * 100) / (n_nchash / 100);
	error = SYSCTL_OUT(req, &n_nchash, sizeof(n_nchash));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &used, sizeof(used));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &maxlength, sizeof(maxlength));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &pct, sizeof(pct));
	if (error)
		return (error);
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, nchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_nchash, "I",
    "nchash statistics (number of total/used buckets, maximum chain length, usage percentage)");
#endif

/*
 * cache_zap():
 *
 *   Removes a namecache entry from cache, whether it contains an actual
 *   pointer to a vnode or if it is just a negative cache entry.
 */
static void
cache_zap(struct namecache *ncp)
{
	struct vnode *vp;

	rw_assert(&cache_lock, RA_WLOCKED);
	CTR2(KTR_VFS, "cache_zap(%p) vp %p", ncp, ncp->nc_vp);
	if (ncp->nc_vp != NULL) {
		SDT_PROBE3(vfs, namecache, zap, done, ncp->nc_dvp,
		    nc_get_name(ncp), ncp->nc_vp);
	} else {
		SDT_PROBE2(vfs, namecache, zap_negative, done, ncp->nc_dvp,
		    nc_get_name(ncp));
	}
	vp = NULL;
	LIST_REMOVE(ncp, nc_hash);
	if (ncp->nc_flag & NCF_ISDOTDOT) {
		if (ncp == ncp->nc_dvp->v_cache_dd)
			ncp->nc_dvp->v_cache_dd = NULL;
	} else {
		LIST_REMOVE(ncp, nc_src);
		if (LIST_EMPTY(&ncp->nc_dvp->v_cache_src)) {
			vp = ncp->nc_dvp;
			numcachehv--;
		}
	}
	if (ncp->nc_vp) {
		TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst, ncp, nc_dst);
		if (ncp == ncp->nc_vp->v_cache_dd)
			ncp->nc_vp->v_cache_dd = NULL;
	} else {
		TAILQ_REMOVE(&ncneg, ncp, nc_dst);
		numneg--;
	}
	numcache--;
	cache_free(ncp);
	if (vp != NULL)
		vdrop(vp);
}

/*
 * Lookup an entry in the cache
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought. If the lookup
 * succeeds, the vnode is returned in *vpp, and a status of -1 is
 * returned. If the lookup determines that the name does not exist
 * (negative caching), a status of ENOENT is returned. If the lookup
 * fails, a status of zero is returned.  If the directory vnode is
 * recycled out from under us due to a forced unmount, a status of
 * ENOENT is returned.
 *
 * vpp is locked and ref'd on return.  If we're looking up DOTDOT, dvp is
 * unlocked.  If we're looking up . an extra ref is taken, but the lock is
 * not recursively acquired.
 */

int
cache_lookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	struct namecache *ncp;
	uint32_t hash;
	int error, ltype, wlocked;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}
retry:
	wlocked = 0;
	counter_u64_add(numcalls, 1);
	error = 0;

retry_wlocked:
	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1) {
			*vpp = dvp;
			CTR2(KTR_VFS, "cache_lookup(%p, %s) found via .",
			    dvp, cnp->cn_nameptr);
			counter_u64_add(dothits, 1);
			SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ".", *vpp);
			if (tsp != NULL)
				timespecclear(tsp);
			if (ticksp != NULL)
				*ticksp = ticks;
			VREF(*vpp);
			/*
			 * When we lookup "." we still can be asked to lock it
			 * differently...
			 */
			ltype = cnp->cn_lkflags & LK_TYPE_MASK;
			if (ltype != VOP_ISLOCKED(*vpp)) {
				if (ltype == LK_EXCLUSIVE) {
					vn_lock(*vpp, LK_UPGRADE | LK_RETRY);
					if ((*vpp)->v_iflag & VI_DOOMED) {
						/* forced unmount */
						vrele(*vpp);
						*vpp = NULL;
						return (ENOENT);
					}
				} else
					vn_lock(*vpp, LK_DOWNGRADE | LK_RETRY);
			}
			return (-1);
		}
		if (!wlocked)
			CACHE_RLOCK();
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			counter_u64_add(dotdothits, 1);
			if (dvp->v_cache_dd == NULL) {
				SDT_PROBE3(vfs, namecache, lookup, miss, dvp,
				    "..", NULL);
				goto unlock;
			}
			if ((cnp->cn_flags & MAKEENTRY) == 0) {
				if (!wlocked && !CACHE_UPGRADE_LOCK())
					goto wlock;
				if (dvp->v_cache_dd->nc_flag & NCF_ISDOTDOT)
					cache_zap(dvp->v_cache_dd);
				dvp->v_cache_dd = NULL;
				CACHE_WUNLOCK();
				return (0);
			}
			ncp = dvp->v_cache_dd;
			if (ncp->nc_flag & NCF_ISDOTDOT)
				*vpp = ncp->nc_vp;
			else
				*vpp = ncp->nc_dvp;
			/* Return failure if negative entry was found. */
			if (*vpp == NULL)
				goto negative_success;
			CTR3(KTR_VFS, "cache_lookup(%p, %s) found %p via ..",
			    dvp, cnp->cn_nameptr, *vpp);
			SDT_PROBE3(vfs, namecache, lookup, hit, dvp, "..",
			    *vpp);
			cache_out_ts(ncp, tsp, ticksp);
			if ((ncp->nc_flag & (NCF_ISDOTDOT | NCF_DTS)) ==
			    NCF_DTS && tsp != NULL)
				*tsp = ((struct namecache_ts *)ncp)->
				    nc_dotdottime;
			goto success;
		}
	} else if (!wlocked)
		CACHE_RLOCK();

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	LIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		counter_u64_add(numchecks, 1);
		if (ncp->nc_dvp == dvp && ncp->nc_nlen == cnp->cn_namelen &&
		    !bcmp(nc_get_name(ncp), cnp->cn_nameptr, ncp->nc_nlen))
			break;
	}

	/* We failed to find an entry */
	if (ncp == NULL) {
		SDT_PROBE3(vfs, namecache, lookup, miss, dvp, cnp->cn_nameptr,
		    NULL);
		if ((cnp->cn_flags & MAKEENTRY) == 0) {
			counter_u64_add(nummisszap, 1);
		} else {
			counter_u64_add(nummiss, 1);
		}
		goto unlock;
	}

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		counter_u64_add(numposzaps, 1);
		if (!wlocked && !CACHE_UPGRADE_LOCK())
			goto wlock;
		cache_zap(ncp);
		CACHE_WUNLOCK();
		return (0);
	}

	/* We found a "positive" match, return the vnode */
	if (ncp->nc_vp) {
		counter_u64_add(numposhits, 1);
		*vpp = ncp->nc_vp;
		CTR4(KTR_VFS, "cache_lookup(%p, %s) found %p via ncp %p",
		    dvp, cnp->cn_nameptr, *vpp, ncp);
		SDT_PROBE3(vfs, namecache, lookup, hit, dvp, nc_get_name(ncp),
		    *vpp);
		cache_out_ts(ncp, tsp, ticksp);
		goto success;
	}

negative_success:
	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE) {
		counter_u64_add(numnegzaps, 1);
		if (!wlocked && !CACHE_UPGRADE_LOCK())
			goto wlock;
		cache_zap(ncp);
		CACHE_WUNLOCK();
		return (0);
	}

	if (!wlocked && !CACHE_UPGRADE_LOCK())
		goto wlock;
	counter_u64_add(numneghits, 1);
	/*
	 * We found a "negative" match, so we shift it to the end of
	 * the "negative" cache entries queue to satisfy LRU.  Also,
	 * check to see if the entry is a whiteout; indicate this to
	 * the componentname, if so.
	 */
	TAILQ_REMOVE(&ncneg, ncp, nc_dst);
	TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
	if (ncp->nc_flag & NCF_WHITE)
		cnp->cn_flags |= ISWHITEOUT;
	SDT_PROBE2(vfs, namecache, lookup, hit__negative, dvp,
	    nc_get_name(ncp));
	cache_out_ts(ncp, tsp, ticksp);
	CACHE_WUNLOCK();
	return (ENOENT);

wlock:
	/*
	 * We need to update the cache after our lookup, so upgrade to
	 * a write lock and retry the operation.
	 */
	CACHE_RUNLOCK();
	CACHE_WLOCK();
	numupgrades++;
	wlocked = 1;
	goto retry_wlocked;

success:
	/*
	 * On success we return a locked and ref'd vnode as per the lookup
	 * protocol.
	 */
	MPASS(dvp != *vpp);
	ltype = 0;	/* silence gcc warning */
	if (cnp->cn_flags & ISDOTDOT) {
		ltype = VOP_ISLOCKED(dvp);
		VOP_UNLOCK(dvp, 0);
	}
	vhold(*vpp);
	if (wlocked)
		CACHE_WUNLOCK();
	else
		CACHE_RUNLOCK();
	error = vget(*vpp, cnp->cn_lkflags | LK_VNHELD, cnp->cn_thread);
	if (cnp->cn_flags & ISDOTDOT) {
		vn_lock(dvp, ltype | LK_RETRY);
		if (dvp->v_iflag & VI_DOOMED) {
			if (error == 0)
				vput(*vpp);
			*vpp = NULL;
			return (ENOENT);
		}
	}
	if (error) {
		*vpp = NULL;
		goto retry;
	}
	if ((cnp->cn_flags & ISLASTCN) &&
	    (cnp->cn_lkflags & LK_TYPE_MASK) == LK_EXCLUSIVE) {
		ASSERT_VOP_ELOCKED(*vpp, "cache_lookup");
	}
	return (-1);

unlock:
	if (wlocked)
		CACHE_WUNLOCK();
	else
		CACHE_RUNLOCK();
	return (0);
}

/*
 * Add an entry to the cache.
 */
void
cache_enter_time(struct vnode *dvp, struct vnode *vp, struct componentname *cnp,
    struct timespec *tsp, struct timespec *dtsp)
{
	struct namecache *ncp, *n2;
	struct namecache_ts *n3;
	struct nchashhead *ncpp;
	uint32_t hash;
	int flag;
	int len;

	CTR3(KTR_VFS, "cache_enter(%p, %p, %s)", dvp, vp, cnp->cn_nameptr);
	VNASSERT(vp == NULL || (vp->v_iflag & VI_DOOMED) == 0, vp,
	    ("cache_enter: Adding a doomed vnode"));
	VNASSERT(dvp == NULL || (dvp->v_iflag & VI_DOOMED) == 0, dvp,
	    ("cache_enter: Doomed vnode used as src"));

	if (!doingcache)
		return;

	/*
	 * Avoid blowout in namecache entries.
	 */
	if (numcache >= desiredvnodes * ncsizefactor)
		return;

	flag = 0;
	if (cnp->cn_nameptr[0] == '.') {
		if (cnp->cn_namelen == 1)
			return;
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			CACHE_WLOCK();
			/*
			 * If dotdot entry already exists, just retarget it
			 * to new parent vnode, otherwise continue with new
			 * namecache entry allocation.
			 */
			if ((ncp = dvp->v_cache_dd) != NULL &&
			    ncp->nc_flag & NCF_ISDOTDOT) {
				KASSERT(ncp->nc_dvp == dvp,
				    ("wrong isdotdot parent"));
				if (ncp->nc_vp != NULL) {
					TAILQ_REMOVE(&ncp->nc_vp->v_cache_dst,
					    ncp, nc_dst);
				} else {
					TAILQ_REMOVE(&ncneg, ncp, nc_dst);
					numneg--;
				}
				if (vp != NULL) {
					TAILQ_INSERT_HEAD(&vp->v_cache_dst,
					    ncp, nc_dst);
				} else {
					TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
					numneg++;
				}
				ncp->nc_vp = vp;
				CACHE_WUNLOCK();
				return;
			}
			dvp->v_cache_dd = NULL;
			SDT_PROBE3(vfs, namecache, enter, done, dvp, "..", vp);
			CACHE_WUNLOCK();
			flag = NCF_ISDOTDOT;
		}
	}

	/*
	 * Calculate the hash key and setup as much of the new
	 * namecache entry as possible before acquiring the lock.
	 */
	ncp = cache_alloc(cnp->cn_namelen, tsp != NULL);
	ncp->nc_vp = vp;
	ncp->nc_dvp = dvp;
	ncp->nc_flag = flag;
	if (tsp != NULL) {
		n3 = (struct namecache_ts *)ncp;
		n3->nc_time = *tsp;
		n3->nc_ticks = ticks;
		n3->nc_flag |= NCF_TS;
		if (dtsp != NULL) {
			n3->nc_dotdottime = *dtsp;
			n3->nc_flag |= NCF_DTS;
		}
	}
	len = ncp->nc_nlen = cnp->cn_namelen;
	hash = cache_get_hash(cnp->cn_nameptr, len, dvp);
	strlcpy(nc_get_name(ncp), cnp->cn_nameptr, len + 1);
	CACHE_WLOCK();

	/*
	 * See if this vnode or negative entry is already in the cache
	 * with this name.  This can happen with concurrent lookups of
	 * the same path name.
	 */
	ncpp = NCHHASH(hash);
	LIST_FOREACH(n2, ncpp, nc_hash) {
		if (n2->nc_dvp == dvp &&
		    n2->nc_nlen == cnp->cn_namelen &&
		    !bcmp(nc_get_name(n2), cnp->cn_nameptr, n2->nc_nlen)) {
			if (tsp != NULL) {
				KASSERT((n2->nc_flag & NCF_TS) != 0,
				    ("no NCF_TS"));
				n3 = (struct namecache_ts *)n2;
				n3->nc_time =
				    ((struct namecache_ts *)ncp)->nc_time;
				n3->nc_ticks =
				    ((struct namecache_ts *)ncp)->nc_ticks;
				if (dtsp != NULL) {
					n3->nc_dotdottime =
					    ((struct namecache_ts *)ncp)->
					    nc_dotdottime;
					n3->nc_flag |= NCF_DTS;
				}
			}
			CACHE_WUNLOCK();
			cache_free(ncp);
			return;
		}
	}

	if (flag == NCF_ISDOTDOT) {
		/*
		 * See if we are trying to add .. entry, but some other lookup
		 * has populated v_cache_dd pointer already.
		 */
		if (dvp->v_cache_dd != NULL) {
			CACHE_WUNLOCK();
			cache_free(ncp);
			return;
		}
		KASSERT(vp == NULL || vp->v_type == VDIR,
		    ("wrong vnode type %p", vp));
		dvp->v_cache_dd = ncp;
	}

	numcache++;
	if (vp != NULL) {
		if (vp->v_type == VDIR) {
			if (flag != NCF_ISDOTDOT) {
				/*
				 * For this case, the cache entry maps both the
				 * directory name in it and the name ".." for the
				 * directory's parent.
				 */
				if ((n2 = vp->v_cache_dd) != NULL &&
				    (n2->nc_flag & NCF_ISDOTDOT) != 0)
					cache_zap(n2);
				vp->v_cache_dd = ncp;
			}
		} else {
			vp->v_cache_dd = NULL;
		}
	}

	/*
	 * Insert the new namecache entry into the appropriate chain
	 * within the cache entries table.
	 */
	LIST_INSERT_HEAD(ncpp, ncp, nc_hash);
	if (flag != NCF_ISDOTDOT) {
		if (LIST_EMPTY(&dvp->v_cache_src)) {
			vhold(dvp);
			numcachehv++;
		}
		LIST_INSERT_HEAD(&dvp->v_cache_src, ncp, nc_src);
	}

	/*
	 * If the entry is "negative", we place it into the
	 * "negative" cache queue, otherwise, we place it into the
	 * destination vnode's cache entries queue.
	 */
	if (vp != NULL) {
		TAILQ_INSERT_HEAD(&vp->v_cache_dst, ncp, nc_dst);
		SDT_PROBE3(vfs, namecache, enter, done, dvp, nc_get_name(ncp),
		    vp);
	} else {
		if (cnp->cn_flags & ISWHITEOUT)
			ncp->nc_flag |= NCF_WHITE;
		TAILQ_INSERT_TAIL(&ncneg, ncp, nc_dst);
		numneg++;
		SDT_PROBE2(vfs, namecache, enter_negative, done, dvp,
		    nc_get_name(ncp));
	}
	if (numneg * ncnegfactor > numcache) {
		ncp = TAILQ_FIRST(&ncneg);
		KASSERT(ncp->nc_vp == NULL, ("ncp %p vp %p on ncneg",
		    ncp, ncp->nc_vp));
		cache_zap(ncp);
	}
	CACHE_WUNLOCK();
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
static void
nchinit(void *dummy __unused)
{

	TAILQ_INIT(&ncneg);

	cache_zone_small = uma_zcreate("S VFS Cache",
	    sizeof(struct namecache) + CACHE_PATH_CUTOFF + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cache_zone_small_ts = uma_zcreate("STS VFS Cache",
	    sizeof(struct namecache_ts) + CACHE_PATH_CUTOFF + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cache_zone_large = uma_zcreate("L VFS Cache",
	    sizeof(struct namecache) + NAME_MAX + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);
	cache_zone_large_ts = uma_zcreate("LTS VFS Cache",
	    sizeof(struct namecache_ts) + NAME_MAX + 1,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_ZINIT);

	nchashtbl = hashinit(desiredvnodes * 2, M_VFSCACHE, &nchash);

	numcalls = counter_u64_alloc(M_WAITOK);
	dothits = counter_u64_alloc(M_WAITOK);
	dotdothits = counter_u64_alloc(M_WAITOK);
	numchecks = counter_u64_alloc(M_WAITOK);
	nummiss = counter_u64_alloc(M_WAITOK);
	nummisszap = counter_u64_alloc(M_WAITOK);
	numposzaps = counter_u64_alloc(M_WAITOK);
	numposhits = counter_u64_alloc(M_WAITOK);
	numnegzaps = counter_u64_alloc(M_WAITOK);
	numneghits = counter_u64_alloc(M_WAITOK);
	numfullpathcalls = counter_u64_alloc(M_WAITOK);
	numfullpathfail1 = counter_u64_alloc(M_WAITOK);
	numfullpathfail2 = counter_u64_alloc(M_WAITOK);
	numfullpathfail4 = counter_u64_alloc(M_WAITOK);
	numfullpathfound = counter_u64_alloc(M_WAITOK);
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nchinit, NULL);

void
cache_changesize(int newmaxvnodes)
{
	struct nchashhead *new_nchashtbl, *old_nchashtbl;
	u_long new_nchash, old_nchash;
	struct namecache *ncp;
	uint32_t hash;
	int i;

	new_nchashtbl = hashinit(newmaxvnodes * 2, M_VFSCACHE, &new_nchash);
	/* If same hash table size, nothing to do */
	if (nchash == new_nchash) {
		free(new_nchashtbl, M_VFSCACHE);
		return;
	}
	/*
	 * Move everything from the old hash table to the new table.
	 * None of the namecache entries in the table can be removed
	 * because to do so, they have to be removed from the hash table.
	 */
	CACHE_WLOCK();
	old_nchashtbl = nchashtbl;
	old_nchash = nchash;
	nchashtbl = new_nchashtbl;
	nchash = new_nchash;
	for (i = 0; i <= old_nchash; i++) {
		while ((ncp = LIST_FIRST(&old_nchashtbl[i])) != NULL) {
			hash = cache_get_hash(nc_get_name(ncp), ncp->nc_nlen,
			    ncp->nc_dvp);
			LIST_REMOVE(ncp, nc_hash);
			LIST_INSERT_HEAD(NCHHASH(hash), ncp, nc_hash);
		}
	}
	CACHE_WUNLOCK();
	free(old_nchashtbl, M_VFSCACHE);
}

/*
 * Invalidate all entries to a particular vnode.
 */
void
cache_purge(struct vnode *vp)
{

	CTR1(KTR_VFS, "cache_purge(%p)", vp);
	SDT_PROBE1(vfs, namecache, purge, done, vp);
	CACHE_WLOCK();
	while (!LIST_EMPTY(&vp->v_cache_src))
		cache_zap(LIST_FIRST(&vp->v_cache_src));
	while (!TAILQ_EMPTY(&vp->v_cache_dst))
		cache_zap(TAILQ_FIRST(&vp->v_cache_dst));
	if (vp->v_cache_dd != NULL) {
		KASSERT(vp->v_cache_dd->nc_flag & NCF_ISDOTDOT,
		   ("lost dotdot link"));
		cache_zap(vp->v_cache_dd);
	}
	KASSERT(vp->v_cache_dd == NULL, ("incomplete purge"));
	CACHE_WUNLOCK();
}

/*
 * Invalidate all negative entries for a particular directory vnode.
 */
void
cache_purge_negative(struct vnode *vp)
{
	struct namecache *cp, *ncp;

	CTR1(KTR_VFS, "cache_purge_negative(%p)", vp);
	SDT_PROBE1(vfs, namecache, purge_negative, done, vp);
	CACHE_WLOCK();
	LIST_FOREACH_SAFE(cp, &vp->v_cache_src, nc_src, ncp) {
		if (cp->nc_vp == NULL)
			cache_zap(cp);
	}
	CACHE_WUNLOCK();
}

/*
 * Flush all entries referencing a particular filesystem.
 */
void
cache_purgevfs(struct mount *mp)
{
	struct nchashhead *ncpp;
	struct namecache *ncp, *nnp;

	/* Scan hash tables for applicable entries */
	SDT_PROBE1(vfs, namecache, purgevfs, done, mp);
	CACHE_WLOCK();
	for (ncpp = &nchashtbl[nchash]; ncpp >= nchashtbl; ncpp--) {
		LIST_FOREACH_SAFE(ncp, ncpp, nc_hash, nnp) {
			if (ncp->nc_dvp->v_mount == mp)
				cache_zap(ncp);
		}
	}
	CACHE_WUNLOCK();
}

/*
 * Perform canonical checks and cache lookup and pass on to filesystem
 * through the vop_cachedlookup only if needed.
 */

int
vfs_cache_lookup(struct vop_lookup_args *ap)
{
	struct vnode *dvp;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int flags = cnp->cn_flags;
	struct thread *td = cnp->cn_thread;

	*vpp = NULL;
	dvp = ap->a_dvp;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = VOP_ACCESS(dvp, VEXEC, cred, td);
	if (error)
		return (error);

	error = cache_lookup(dvp, vpp, cnp, NULL, NULL);
	if (error == 0)
		return (VOP_CACHEDLOOKUP(dvp, vpp, cnp));
	if (error == -1)
		return (0);
	return (error);
}

/*
 * XXX All of these sysctls would probably be more productive dead.
 */
static int disablecwd;
SYSCTL_INT(_debug, OID_AUTO, disablecwd, CTLFLAG_RW, &disablecwd, 0,
   "Disable the getcwd syscall");

/* Implementation of the getcwd syscall. */
int
sys___getcwd(struct thread *td, struct __getcwd_args *uap)
{

	return (kern___getcwd(td, uap->buf, UIO_USERSPACE, uap->buflen,
	    MAXPATHLEN));
}

int
kern___getcwd(struct thread *td, char *buf, enum uio_seg bufseg, u_int buflen,
    u_int path_max)
{
	char *bp, *tmpbuf;
	struct filedesc *fdp;
	struct vnode *cdir, *rdir;
	int error;

	if (disablecwd)
		return (ENODEV);
	if (buflen < 2)
		return (EINVAL);
	if (buflen > path_max)
		buflen = path_max;

	tmpbuf = malloc(buflen, M_TEMP, M_WAITOK);
	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	cdir = fdp->fd_cdir;
	VREF(cdir);
	rdir = fdp->fd_rdir;
	VREF(rdir);
	FILEDESC_SUNLOCK(fdp);
	error = vn_fullpath1(td, cdir, rdir, tmpbuf, &bp, buflen);
	vrele(rdir);
	vrele(cdir);

	if (!error) {
		if (bufseg == UIO_SYSSPACE)
			bcopy(bp, buf, strlen(bp) + 1);
		else
			error = copyout(bp, buf, strlen(bp) + 1);
#ifdef KTRACE
	if (KTRPOINT(curthread, KTR_NAMEI))
		ktrnamei(bp);
#endif
	}
	free(tmpbuf, M_TEMP);
	return (error);
}

/*
 * Thus begins the fullpath magic.
 */

static int disablefullpath;
SYSCTL_INT(_debug, OID_AUTO, disablefullpath, CTLFLAG_RW, &disablefullpath, 0,
    "Disable the vn_fullpath function");

/*
 * Retrieve the full filesystem path that correspond to a vnode from the name
 * cache (if available)
 */
int
vn_fullpath(struct thread *td, struct vnode *vn, char **retbuf, char **freebuf)
{
	char *buf;
	struct filedesc *fdp;
	struct vnode *rdir;
	int error;

	if (disablefullpath)
		return (ENODEV);
	if (vn == NULL)
		return (EINVAL);

	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	rdir = fdp->fd_rdir;
	VREF(rdir);
	FILEDESC_SUNLOCK(fdp);
	error = vn_fullpath1(td, vn, rdir, buf, retbuf, MAXPATHLEN);
	vrele(rdir);

	if (!error)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

/*
 * This function is similar to vn_fullpath, but it attempts to lookup the
 * pathname relative to the global root mount point.  This is required for the
 * auditing sub-system, as audited pathnames must be absolute, relative to the
 * global root mount point.
 */
int
vn_fullpath_global(struct thread *td, struct vnode *vn,
    char **retbuf, char **freebuf)
{
	char *buf;
	int error;

	if (disablefullpath)
		return (ENODEV);
	if (vn == NULL)
		return (EINVAL);
	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	error = vn_fullpath1(td, vn, rootvnode, buf, retbuf, MAXPATHLEN);
	if (!error)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

int
vn_vptocnp(struct vnode **vp, struct ucred *cred, char *buf, u_int *buflen)
{
	int error;

	CACHE_RLOCK();
	error = vn_vptocnp_locked(vp, cred, buf, buflen);
	if (error == 0)
		CACHE_RUNLOCK();
	return (error);
}

static int
vn_vptocnp_locked(struct vnode **vp, struct ucred *cred, char *buf,
    u_int *buflen)
{
	struct vnode *dvp;
	struct namecache *ncp;
	int error;

	TAILQ_FOREACH(ncp, &((*vp)->v_cache_dst), nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
	}
	if (ncp != NULL) {
		if (*buflen < ncp->nc_nlen) {
			CACHE_RUNLOCK();
			vrele(*vp);
			counter_u64_add(numfullpathfail4, 1);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    vp, NULL);
			return (error);
		}
		*buflen -= ncp->nc_nlen;
		memcpy(buf + *buflen, nc_get_name(ncp), ncp->nc_nlen);
		SDT_PROBE3(vfs, namecache, fullpath, hit, ncp->nc_dvp,
		    nc_get_name(ncp), vp);
		dvp = *vp;
		*vp = ncp->nc_dvp;
		vref(*vp);
		CACHE_RUNLOCK();
		vrele(dvp);
		CACHE_RLOCK();
		return (0);
	}
	SDT_PROBE1(vfs, namecache, fullpath, miss, vp);

	CACHE_RUNLOCK();
	vn_lock(*vp, LK_SHARED | LK_RETRY);
	error = VOP_VPTOCNP(*vp, &dvp, cred, buf, buflen);
	vput(*vp);
	if (error) {
		counter_u64_add(numfullpathfail2, 1);
		SDT_PROBE3(vfs, namecache, fullpath, return,  error, vp, NULL);
		return (error);
	}

	*vp = dvp;
	CACHE_RLOCK();
	if (dvp->v_iflag & VI_DOOMED) {
		/* forced unmount */
		CACHE_RUNLOCK();
		vrele(dvp);
		error = ENOENT;
		SDT_PROBE3(vfs, namecache, fullpath, return, error, vp, NULL);
		return (error);
	}
	/*
	 * *vp has its use count incremented still.
	 */

	return (0);
}

/*
 * The magic behind kern___getcwd() and vn_fullpath().
 */
static int
vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen)
{
	int error, slash_prefixed;
#ifdef KDTRACE_HOOKS
	struct vnode *startvp = vp;
#endif
	struct vnode *vp1;

	buflen--;
	buf[buflen] = '\0';
	error = 0;
	slash_prefixed = 0;

	SDT_PROBE1(vfs, namecache, fullpath, entry, vp);
	counter_u64_add(numfullpathcalls, 1);
	vref(vp);
	CACHE_RLOCK();
	if (vp->v_type != VDIR) {
		error = vn_vptocnp_locked(&vp, td->td_ucred, buf, &buflen);
		if (error)
			return (error);
		if (buflen == 0) {
			CACHE_RUNLOCK();
			vrele(vp);
			return (ENOMEM);
		}
		buf[--buflen] = '/';
		slash_prefixed = 1;
	}
	while (vp != rdir && vp != rootvnode) {
		if (vp->v_vflag & VV_ROOT) {
			if (vp->v_iflag & VI_DOOMED) {	/* forced unmount */
				CACHE_RUNLOCK();
				vrele(vp);
				error = ENOENT;
				SDT_PROBE3(vfs, namecache, fullpath, return,
				    error, vp, NULL);
				break;
			}
			vp1 = vp->v_mount->mnt_vnodecovered;
			vref(vp1);
			CACHE_RUNLOCK();
			vrele(vp);
			vp = vp1;
			CACHE_RLOCK();
			continue;
		}
		if (vp->v_type != VDIR) {
			CACHE_RUNLOCK();
			vrele(vp);
			counter_u64_add(numfullpathfail1, 1);
			error = ENOTDIR;
			SDT_PROBE3(vfs, namecache, fullpath, return,
			    error, vp, NULL);
			break;
		}
		error = vn_vptocnp_locked(&vp, td->td_ucred, buf, &buflen);
		if (error)
			break;
		if (buflen == 0) {
			CACHE_RUNLOCK();
			vrele(vp);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    startvp, NULL);
			break;
		}
		buf[--buflen] = '/';
		slash_prefixed = 1;
	}
	if (error)
		return (error);
	if (!slash_prefixed) {
		if (buflen == 0) {
			CACHE_RUNLOCK();
			vrele(vp);
			counter_u64_add(numfullpathfail4, 1);
			SDT_PROBE3(vfs, namecache, fullpath, return, ENOMEM,
			    startvp, NULL);
			return (ENOMEM);
		}
		buf[--buflen] = '/';
	}
	counter_u64_add(numfullpathfound, 1);
	CACHE_RUNLOCK();
	vrele(vp);

	SDT_PROBE3(vfs, namecache, fullpath, return, 0, startvp, buf + buflen);
	*retbuf = buf + buflen;
	return (0);
}

struct vnode *
vn_dir_dd_ino(struct vnode *vp)
{
	struct namecache *ncp;
	struct vnode *ddvp;

	ASSERT_VOP_LOCKED(vp, "vn_dir_dd_ino");
	CACHE_RLOCK();
	TAILQ_FOREACH(ncp, &(vp->v_cache_dst), nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0)
			continue;
		ddvp = ncp->nc_dvp;
		vhold(ddvp);
		CACHE_RUNLOCK();
		if (vget(ddvp, LK_SHARED | LK_NOWAIT | LK_VNHELD, curthread))
			return (NULL);
		return (ddvp);
	}
	CACHE_RUNLOCK();
	return (NULL);
}

int
vn_commname(struct vnode *vp, char *buf, u_int buflen)
{
	struct namecache *ncp;
	int l;

	CACHE_RLOCK();
	TAILQ_FOREACH(ncp, &vp->v_cache_dst, nc_dst)
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
	if (ncp == NULL) {
		CACHE_RUNLOCK();
		return (ENOENT);
	}
	l = min(ncp->nc_nlen, buflen - 1);
	memcpy(buf, nc_get_name(ncp), l);
	CACHE_RUNLOCK();
	buf[l] = '\0';
	return (0);
}

/* ABI compat shims for old kernel modules. */
#undef cache_enter

void	cache_enter(struct vnode *dvp, struct vnode *vp,
	    struct componentname *cnp);

void
cache_enter(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{

	cache_enter_time(dvp, vp, cnp, NULL, NULL);
}

/*
 * This function updates path string to vnode's full global path
 * and checks the size of the new path string against the pathlen argument.
 *
 * Requires a locked, referenced vnode.
 * Vnode is re-locked on success or ENODEV, otherwise unlocked.
 *
 * If sysctl debug.disablefullpath is set, ENODEV is returned,
 * vnode is left locked and path remain untouched.
 *
 * If vp is a directory, the call to vn_fullpath_global() always succeeds
 * because it falls back to the ".." lookup if the namecache lookup fails.
 */
int
vn_path_to_global_path(struct thread *td, struct vnode *vp, char *path,
    u_int pathlen)
{
	struct nameidata nd;
	struct vnode *vp1;
	char *rpath, *fbuf;
	int error;

	ASSERT_VOP_ELOCKED(vp, __func__);

	/* Return ENODEV if sysctl debug.disablefullpath==1 */
	if (disablefullpath)
		return (ENODEV);

	/* Construct global filesystem path from vp. */
	VOP_UNLOCK(vp, 0);
	error = vn_fullpath_global(td, vp, &rpath, &fbuf);

	if (error != 0) {
		vrele(vp);
		return (error);
	}

	if (strlen(rpath) >= pathlen) {
		vrele(vp);
		error = ENAMETOOLONG;
		goto out;
	}

	/*
	 * Re-lookup the vnode by path to detect a possible rename.
	 * As a side effect, the vnode is relocked.
	 * If vnode was renamed, return ENOENT.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1,
	    UIO_SYSSPACE, path, td);
	error = namei(&nd);
	if (error != 0) {
		vrele(vp);
		goto out;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp1 = nd.ni_vp;
	vrele(vp);
	if (vp1 == vp)
		strcpy(path, rpath);
	else {
		vput(vp1);
		error = ENOENT;
	}

out:
	free(fbuf, M_TEMP);
	return (error);
}
