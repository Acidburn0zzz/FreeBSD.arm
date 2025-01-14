/*
 * Copyright (c) 1983, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsstat.c	8.2 (Berkeley) 3/31/95";
#endif
static const char rcsid[] =
  "$FreeBSD: head/usr.bin/nfsstat/nfsstat.c 292686 2015-12-24 11:41:21Z bapt $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsserver/nfs.h>
#include <nfs/nfssvc.h>

#include <fs/nfs/nfsport.h>

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>

static int widemode = 0;
static int zflag = 0;
static int printtitle = 1;
static struct ext_nfsstats ext_nfsstats;
static int extra_output = 0;

static void intpr(int, int);
static void printhdr(int, int);
static void usage(void);
static char *sperc1(int, int);
static char *sperc2(int, int);
static void exp_intpr(int, int);
static void exp_sidewaysintpr(u_int, int, int);

#define DELTA(field)	(nfsstats.field - lastst.field)

int
main(int argc, char **argv)
{
	u_int interval;
	int clientOnly = -1;
	int serverOnly = -1;
	int ch;
	char *memf, *nlistf;
	int mntlen, i;
	char buf[1024];
	struct statfs *mntbuf;
	struct nfscl_dumpmntopts dumpmntopts;

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "cesWM:mN:w:z")) != -1)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'm':
			/* Display mount options for NFS mount points. */
			mntlen = getmntinfo(&mntbuf, MNT_NOWAIT);
			for (i = 0; i < mntlen; i++) {
				if (strcmp(mntbuf->f_fstypename, "nfs") == 0) {
					dumpmntopts.ndmnt_fname =
					    mntbuf->f_mntonname;
					dumpmntopts.ndmnt_buf = buf;
					dumpmntopts.ndmnt_blen = sizeof(buf);
					if (nfssvc(NFSSVC_DUMPMNTOPTS,
					    &dumpmntopts) >= 0)
						printf("%s on %s\n%s\n",
						    mntbuf->f_mntfromname,
						    mntbuf->f_mntonname, buf);
					else if (errno == EPERM)
						errx(1, "Only priviledged users"
						    " can use the -m option");
				}
				mntbuf++;
			}
			exit(0);
		case 'N':
			nlistf = optarg;
			break;
		case 'W':
			widemode = 1;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case 'c':
			clientOnly = 1;
			if (serverOnly < 0)
				serverOnly = 0;
			break;
		case 's':
			serverOnly = 1;
			if (clientOnly < 0)
				clientOnly = 0;
			break;
		case 'z':
			zflag = 1;
			break;
		case 'e':
			extra_output = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif
	if (modfind("nfscommon") < 0)
		errx(1, "NFS client/server not loaded");

	if (interval) {
		exp_sidewaysintpr(interval, clientOnly, serverOnly);
	} else {
		if (extra_output != 0)
			exp_intpr(clientOnly, serverOnly);
		else
			intpr(clientOnly, serverOnly);
	}
	exit(0);
}

/*
 * Print a description of the nfs stats.
 */
static void
intpr(int clientOnly, int serverOnly)
{
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_GETSTATS;
	if (zflag != 0) {
		if (clientOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
		if (serverOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
	}
	if (nfssvc(nfssvc_flag, &ext_nfsstats) < 0)
		err(1, "Can't get stats");
	if (clientOnly) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			ext_nfsstats.rpccnt[NFSPROC_GETATTR],
			ext_nfsstats.rpccnt[NFSPROC_SETATTR],
			ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
			ext_nfsstats.rpccnt[NFSPROC_READLINK],
			ext_nfsstats.rpccnt[NFSPROC_READ],
			ext_nfsstats.rpccnt[NFSPROC_WRITE],
			ext_nfsstats.rpccnt[NFSPROC_CREATE],
			ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			ext_nfsstats.rpccnt[NFSPROC_RENAME],
			ext_nfsstats.rpccnt[NFSPROC_LINK],
			ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
			ext_nfsstats.rpccnt[NFSPROC_MKDIR],
			ext_nfsstats.rpccnt[NFSPROC_RMDIR],
			ext_nfsstats.rpccnt[NFSPROC_READDIR],
			ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
			ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
			ext_nfsstats.rpccnt[NFSPROC_MKNOD],
			ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
			ext_nfsstats.rpccnt[NFSPROC_FSINFO],
			ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
			ext_nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"TimedOut", "Invalid", "X Replies", "Retries", 
			"Requests");
		printf("%9d %9d %9d %9d %9d\n",
			ext_nfsstats.rpctimeouts,
			ext_nfsstats.rpcinvalid,
			ext_nfsstats.rpcunexpected,
			ext_nfsstats.rpcretries,
			ext_nfsstats.rpcrequests);
		printf("Cache Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s",
			"Attr Hits", "Misses", "Lkup Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n",
			"BioR Hits", "Misses", "BioW Hits", "Misses");
		printf("%9d %9d %9d %9d",
			ext_nfsstats.attrcache_hits,
			ext_nfsstats.attrcache_misses,
			ext_nfsstats.lookupcache_hits,
			ext_nfsstats.lookupcache_misses);
		printf(" %9d %9d %9d %9d\n",
			ext_nfsstats.biocache_reads -
			ext_nfsstats.read_bios,
			ext_nfsstats.read_bios,
			ext_nfsstats.biocache_writes -
			ext_nfsstats.write_bios,
			ext_nfsstats.write_bios);
		printf("%9.9s %9.9s %9.9s %9.9s",
			"BioRLHits", "Misses", "BioD Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n", "DirE Hits", "Misses", "Accs Hits", "Misses");
		printf("%9d %9d %9d %9d",
			ext_nfsstats.biocache_readlinks -
			ext_nfsstats.readlink_bios,
			ext_nfsstats.readlink_bios,
			ext_nfsstats.biocache_readdirs -
			ext_nfsstats.readdir_bios,
			ext_nfsstats.readdir_bios);
		printf(" %9d %9d %9d %9d\n",
			ext_nfsstats.direofcache_hits,
			ext_nfsstats.direofcache_misses,
			ext_nfsstats.accesscache_hits,
			ext_nfsstats.accesscache_misses);
	}
	if (serverOnly) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Getattr", "Setattr", "Lookup", "Readlink", "Read",
			"Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
			ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
			ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
			ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
			ext_nfsstats.srvrpccnt[NFSV4OP_READ],
			ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
			ext_nfsstats.srvrpccnt[NFSV4OP_CREATE],
			ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			"Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
			ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
			ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
			ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
			ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
			ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
			ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
			ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
			ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
			ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
			ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
			ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
			ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
			ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT]);
		printf("Server Ret-Failed\n");
		printf("%17d\n", ext_nfsstats.srvrpc_errs);
		printf("Server Faults\n");
		printf("%13d\n", ext_nfsstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
			"Inprog", "Idem", "Non-idem", "Misses");
		printf("%9d %9d %9d %9d\n",
			ext_nfsstats.srvcache_inproghits,
			ext_nfsstats.srvcache_idemdonehits,
			ext_nfsstats.srvcache_nonidemdonehits,
			ext_nfsstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
			"WriteOps", "WriteRPC", "Opsaved");
		/*
		 * The new client doesn't do write gathering. It was
		 * only useful for NFSv2.
		 */
		printf("%9d %9d %9d\n",
			ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
			ext_nfsstats.srvrpccnt[NFSV4OP_WRITE], 0);
	}
}

static void
printhdr(int clientOnly, int serverOnly)
{
	printf("%s%6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s %6.6s",
	    ((serverOnly && clientOnly) ? "        " : " "),
	    "GtAttr", "Lookup", "Rdlink", "Read", "Write", "Rename",
	    "Access", "Rddir");
	if (widemode && clientOnly) {
		printf(" Attr Lkup BioR BioW Accs BioD");
	}
	printf("\n");
	fflush(stdout);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-cemszW] [-M core] [-N system] [-w wait]\n");
	exit(1);
}

static char SPBuf[64][8];
static int SPIndex;

static char * 
sperc1(int hits, int misses)
{
	char *p = SPBuf[SPIndex];

	if (hits + misses) {
		sprintf(p, "%3d%%", 
		    (int)(char)((quad_t)hits * 100 / (hits + misses)));
	} else {
		sprintf(p, "   -");
	}
	SPIndex = (SPIndex + 1) & 63;
	return(p);
}

static char * 
sperc2(int ttl, int misses)
{
	char *p = SPBuf[SPIndex];

	if (ttl) {
		sprintf(p, "%3d%%",
		    (int)(char)((quad_t)(ttl - misses) * 100 / ttl));
	} else {
		sprintf(p, "   -");
	}
	SPIndex = (SPIndex + 1) & 63;
	return(p);
}

/*
 * Print a description of the nfs stats for the experimental client/server.
 */
static void
exp_intpr(int clientOnly, int serverOnly)
{
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_GETSTATS;
	if (zflag != 0) {
		if (clientOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROCLTSTATS;
		if (serverOnly != 0)
			nfssvc_flag |= NFSSVC_ZEROSRVSTATS;
	}
	if (nfssvc(nfssvc_flag, &ext_nfsstats) < 0)
		err(1, "Can't get stats");
	if (clientOnly != 0) {
		if (printtitle) {
			printf("Client Info:\n");
			printf("Rpc Counts:\n");
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Getattr", "Setattr", "Lookup", "Readlink",
			    "Read", "Write", "Create", "Remove");
		}
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_GETATTR],
		    ext_nfsstats.rpccnt[NFSPROC_SETATTR],
		    ext_nfsstats.rpccnt[NFSPROC_LOOKUP],
		    ext_nfsstats.rpccnt[NFSPROC_READLINK],
		    ext_nfsstats.rpccnt[NFSPROC_READ],
		    ext_nfsstats.rpccnt[NFSPROC_WRITE],
		    ext_nfsstats.rpccnt[NFSPROC_CREATE],
		    ext_nfsstats.rpccnt[NFSPROC_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_RENAME],
		    ext_nfsstats.rpccnt[NFSPROC_LINK],
		    ext_nfsstats.rpccnt[NFSPROC_SYMLINK],
		    ext_nfsstats.rpccnt[NFSPROC_MKDIR],
		    ext_nfsstats.rpccnt[NFSPROC_RMDIR],
		    ext_nfsstats.rpccnt[NFSPROC_READDIR],
		    ext_nfsstats.rpccnt[NFSPROC_READDIRPLUS],
		    ext_nfsstats.rpccnt[NFSPROC_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "SetClId", "SetClIdCf", "Lock");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_MKNOD],
		    ext_nfsstats.rpccnt[NFSPROC_FSSTAT],
		    ext_nfsstats.rpccnt[NFSPROC_FSINFO],
		    ext_nfsstats.rpccnt[NFSPROC_PATHCONF],
		    ext_nfsstats.rpccnt[NFSPROC_COMMIT],
		    ext_nfsstats.rpccnt[NFSPROC_SETCLIENTID],
		    ext_nfsstats.rpccnt[NFSPROC_SETCLIENTIDCFRM],
		    ext_nfsstats.rpccnt[NFSPROC_LOCK]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s\n",
			    "LockT", "LockU", "Open", "OpenCfr");
		printf("%9d %9d %9d %9d\n",
		    ext_nfsstats.rpccnt[NFSPROC_LOCKT],
		    ext_nfsstats.rpccnt[NFSPROC_LOCKU],
		    ext_nfsstats.rpccnt[NFSPROC_OPEN],
		    ext_nfsstats.rpccnt[NFSPROC_OPENCONFIRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs", "LocalOwn",
			    "LocalOpen", "LocalLOwn");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.clopenowners,
		    ext_nfsstats.clopens,
		    ext_nfsstats.cllockowners,
		    ext_nfsstats.cllocks,
		    ext_nfsstats.cldelegates,
		    ext_nfsstats.cllocalopenowners,
		    ext_nfsstats.cllocalopens,
		    ext_nfsstats.cllocallockowners);
		if (printtitle)
			printf("%9.9s\n", "LocalLock");
		printf("%9d\n", ext_nfsstats.cllocallocks);
		if (printtitle) {
			printf("Rpc Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "TimedOut", "Invalid", "X Replies", "Retries",
			    "Requests");
		}
		printf("%9d %9d %9d %9d %9d\n",
		    ext_nfsstats.rpctimeouts,
		    ext_nfsstats.rpcinvalid,
		    ext_nfsstats.rpcunexpected,
		    ext_nfsstats.rpcretries,
		    ext_nfsstats.rpcrequests);
		if (printtitle) {
			printf("Cache Info:\n");
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "Attr Hits", "Misses", "Lkup Hits", "Misses");
			printf(" %9.9s %9.9s %9.9s %9.9s\n",
			    "BioR Hits", "Misses", "BioW Hits", "Misses");
		}
		printf("%9d %9d %9d %9d",
		    ext_nfsstats.attrcache_hits,
		    ext_nfsstats.attrcache_misses,
		    ext_nfsstats.lookupcache_hits,
		    ext_nfsstats.lookupcache_misses);
		printf(" %9d %9d %9d %9d\n",
		    ext_nfsstats.biocache_reads - ext_nfsstats.read_bios,
		    ext_nfsstats.read_bios,
		    ext_nfsstats.biocache_writes - ext_nfsstats.write_bios,
		    ext_nfsstats.write_bios);
		if (printtitle) {
			printf("%9.9s %9.9s %9.9s %9.9s",
			    "BioRLHits", "Misses", "BioD Hits", "Misses");
			printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
		}
		printf("%9d %9d %9d %9d",
		    ext_nfsstats.biocache_readlinks -
		    ext_nfsstats.readlink_bios,
		    ext_nfsstats.readlink_bios,
		    ext_nfsstats.biocache_readdirs -
		    ext_nfsstats.readdir_bios,
		    ext_nfsstats.readdir_bios);
		printf(" %9d %9d\n",
		    ext_nfsstats.direofcache_hits,
		    ext_nfsstats.direofcache_misses);
	}
	if (serverOnly != 0) {
		if (printtitle) {
			printf("\nServer Info:\n");
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Getattr", "Setattr", "Lookup", "Readlink",
			    "Read", "Write", "Create", "Remove");
		}
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_GETATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUP],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READLINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READ],
		    ext_nfsstats.srvrpccnt[NFSV4OP_WRITE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_V3CREATE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_REMOVE]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
			    "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_RENAME],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SYMLINK],
		    ext_nfsstats.srvrpccnt[NFSV4OP_MKDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RMDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READDIR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_READDIRPLUS],
		    ext_nfsstats.srvrpccnt[NFSV4OP_ACCESS]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Mknod", "Fsstat", "Fsinfo", "PathConf",
			    "Commit", "LookupP", "SetClId", "SetClIdCf");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_MKNOD],
		    ext_nfsstats.srvrpccnt[NFSV4OP_FSSTAT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_FSINFO],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PATHCONF],
		    ext_nfsstats.srvrpccnt[NFSV4OP_COMMIT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOOKUPP],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTID],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SETCLIENTIDCFRM]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "Open", "OpenAttr", "OpenDwnGr", "OpenCfrm",
			    "DelePurge", "DeleRet", "GetFH", "Lock");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPEN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENATTR],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENDOWNGRADE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_OPENCONFIRM],
		    ext_nfsstats.srvrpccnt[NFSV4OP_DELEGPURGE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_DELEGRETURN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_GETFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCK]);
		if (printtitle)
			printf(
			    "%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n"
			    , "LockT", "LockU", "Close", "Verify", "NVerify",
			    "PutFH", "PutPubFH", "PutRootFH");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCKT],
		    ext_nfsstats.srvrpccnt[NFSV4OP_LOCKU],
		    ext_nfsstats.srvrpccnt[NFSV4OP_CLOSE],
		    ext_nfsstats.srvrpccnt[NFSV4OP_VERIFY],
		    ext_nfsstats.srvrpccnt[NFSV4OP_NVERIFY],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTPUBFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_PUTROOTFH]);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Renew", "RestoreFH", "SaveFH", "Secinfo",
			    "RelLckOwn", "V4Create");
		printf("%9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvrpccnt[NFSV4OP_RENEW],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RESTOREFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SAVEFH],
		    ext_nfsstats.srvrpccnt[NFSV4OP_SECINFO],
		    ext_nfsstats.srvrpccnt[NFSV4OP_RELEASELCKOWN],
		    ext_nfsstats.srvrpccnt[NFSV4OP_CREATE]);
		if (printtitle) {
			printf("Server:\n");
			printf("%9.9s %9.9s %9.9s\n",
			    "Retfailed", "Faults", "Clients");
		}
		printf("%9d %9d %9d\n",
		    ext_nfsstats.srv_errs, ext_nfsstats.srvrpc_errs,
		    ext_nfsstats.srvclients);
		if (printtitle)
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s \n",
			    "OpenOwner", "Opens", "LockOwner",
			    "Locks", "Delegs");
		printf("%9d %9d %9d %9d %9d \n",
		    ext_nfsstats.srvopenowners,
		    ext_nfsstats.srvopens,
		    ext_nfsstats.srvlockowners,
		    ext_nfsstats.srvlocks,
		    ext_nfsstats.srvdelegates);
		if (printtitle) {
			printf("Server Cache Stats:\n");
			printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			    "Inprog", "Idem", "Non-idem", "Misses", 
			    "CacheSize", "TCPPeak");
		}
		printf("%9d %9d %9d %9d %9d %9d\n",
		    ext_nfsstats.srvcache_inproghits,
		    ext_nfsstats.srvcache_idemdonehits,
		    ext_nfsstats.srvcache_nonidemdonehits,
		    ext_nfsstats.srvcache_misses,
		    ext_nfsstats.srvcache_size,
		    ext_nfsstats.srvcache_tcppeak);
	}
}

/*
 * Print a running summary of nfs statistics for the experimental client and/or
 * server.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
exp_sidewaysintpr(u_int interval, int clientOnly, int serverOnly)
{
	struct ext_nfsstats nfsstats, lastst, *ext_nfsstatsp;
	int hdrcnt = 1;

	ext_nfsstatsp = &lastst;
	if (nfssvc(NFSSVC_GETSTATS, ext_nfsstatsp) < 0)
		err(1, "Can't get stats");
	sleep(interval);

	for (;;) {
		ext_nfsstatsp = &nfsstats;
		if (nfssvc(NFSSVC_GETSTATS, ext_nfsstatsp) < 0)
			err(1, "Can't get stats");

		if (--hdrcnt == 0) {
			printhdr(clientOnly, serverOnly);
			if (clientOnly && serverOnly)
				hdrcnt = 10;
			else
				hdrcnt = 20;
		}
		if (clientOnly) {
		    printf("%s %6d %6d %6d %6d %6d %6d %6d %6d",
			((clientOnly && serverOnly) ? "Client:" : ""),
			DELTA(rpccnt[NFSPROC_GETATTR]),
			DELTA(rpccnt[NFSPROC_LOOKUP]),
			DELTA(rpccnt[NFSPROC_READLINK]),
			DELTA(rpccnt[NFSPROC_READ]),
			DELTA(rpccnt[NFSPROC_WRITE]),
			DELTA(rpccnt[NFSPROC_RENAME]),
			DELTA(rpccnt[NFSPROC_ACCESS]),
			DELTA(rpccnt[NFSPROC_READDIR]) +
			DELTA(rpccnt[NFSPROC_READDIRPLUS])
		    );
		    if (widemode) {
			    printf(" %s %s %s %s %s %s",
				sperc1(DELTA(attrcache_hits),
				    DELTA(attrcache_misses)),
				sperc1(DELTA(lookupcache_hits), 
				    DELTA(lookupcache_misses)),
				sperc2(DELTA(biocache_reads),
				    DELTA(read_bios)),
				sperc2(DELTA(biocache_writes),
				    DELTA(write_bios)),
				sperc1(DELTA(accesscache_hits),
				    DELTA(accesscache_misses)),
				sperc2(DELTA(biocache_readdirs),
				    DELTA(readdir_bios))
			    );
		    }
		    printf("\n");
		}
		if (serverOnly) {
		    printf("%s %6d %6d %6d %6d %6d %6d %6d %6d",
			((clientOnly && serverOnly) ? "Server:" : ""),
			DELTA(srvrpccnt[NFSV4OP_GETATTR]),
			DELTA(srvrpccnt[NFSV4OP_LOOKUP]),
			DELTA(srvrpccnt[NFSV4OP_READLINK]),
			DELTA(srvrpccnt[NFSV4OP_READ]),
			DELTA(srvrpccnt[NFSV4OP_WRITE]),
			DELTA(srvrpccnt[NFSV4OP_RENAME]),
			DELTA(srvrpccnt[NFSV4OP_ACCESS]),
			DELTA(srvrpccnt[NFSV4OP_READDIR]) +
			DELTA(srvrpccnt[NFSV4OP_READDIRPLUS]));
		    printf("\n");
		}
		lastst = nfsstats;
		fflush(stdout);
		sleep(interval);
	}
	/*NOTREACHED*/
}
