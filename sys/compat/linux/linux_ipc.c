/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/compat/linux/linux_ipc.c 283416 2015-05-24 15:44:41Z dchagin $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include "opt_compat.h"

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#include <machine/../linux32/linux32_ipc64.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <machine/../linux/linux_ipc64.h>
#endif
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_util.h>

struct l_seminfo {
	l_int semmap;
	l_int semmni;
	l_int semmns;
	l_int semmnu;
	l_int semmsl;
	l_int semopm;
	l_int semume;
	l_int semusz;
	l_int semvmx;
	l_int semaem;
};

struct l_shminfo {
	l_int shmmax;
	l_int shmmin;
	l_int shmmni;
	l_int shmseg;
	l_int shmall;
};

struct l_shm_info {
	l_int used_ids;
	l_ulong shm_tot;  /* total allocated shm */
	l_ulong shm_rss;  /* total resident shm */
	l_ulong shm_swp;  /* total swapped shm */
	l_ulong swap_attempts;
	l_ulong swap_successes;
};

struct l_msginfo {
	l_int msgpool;
	l_int msgmap;
	l_int msgmax;
	l_int msgmnb;
	l_int msgmni;
	l_int msgssz;
	l_int msgtql;
	l_ushort msgseg;
};

static void
bsd_to_linux_shminfo( struct shminfo *bpp, struct l_shminfo *lpp)
{

	lpp->shmmax = bpp->shmmax;
	lpp->shmmin = bpp->shmmin;
	lpp->shmmni = bpp->shmmni;
	lpp->shmseg = bpp->shmseg;
	lpp->shmall = bpp->shmall;
}

static void
bsd_to_linux_shm_info( struct shm_info *bpp, struct l_shm_info *lpp)
{

	lpp->used_ids = bpp->used_ids ;
	lpp->shm_tot = bpp->shm_tot ;
	lpp->shm_rss = bpp->shm_rss ;
	lpp->shm_swp = bpp->shm_swp ;
	lpp->swap_attempts = bpp->swap_attempts ;
	lpp->swap_successes = bpp->swap_successes ;
}

static void
linux_to_bsd_ipc_perm(struct l_ipc_perm *lpp, struct ipc_perm *bpp)
{

	bpp->key = lpp->key;
	bpp->uid = lpp->uid;
	bpp->gid = lpp->gid;
	bpp->cuid = lpp->cuid;
	bpp->cgid = lpp->cgid;
	bpp->mode = lpp->mode;
	bpp->seq = lpp->seq;
}


static void
bsd_to_linux_ipc_perm(struct ipc_perm *bpp, struct l_ipc_perm *lpp)
{

	lpp->key = bpp->key;
	lpp->uid = bpp->uid;
	lpp->gid = bpp->gid;
	lpp->cuid = bpp->cuid;
	lpp->cgid = bpp->cgid;
	lpp->mode = bpp->mode;
	lpp->seq = bpp->seq;
}

struct l_msqid_ds {
	struct l_ipc_perm	msg_perm;
	l_uintptr_t		msg_first;	/* first message on queue,unused */
	l_uintptr_t		msg_last;	/* last message in queue,unused */
	l_time_t		msg_stime;	/* last msgsnd time */
	l_time_t		msg_rtime;	/* last msgrcv time */
	l_time_t		msg_ctime;	/* last change time */
	l_ulong			msg_lcbytes;	/* Reuse junk fields for 32 bit */
	l_ulong			msg_lqbytes;	/* ditto */
	l_ushort		msg_cbytes;	/* current number of bytes on queue */
	l_ushort		msg_qnum;	/* number of messages in queue */
	l_ushort		msg_qbytes;	/* max number of bytes on queue */
	l_pid_t			msg_lspid;	/* pid of last msgsnd */
	l_pid_t			msg_lrpid;	/* last receive pid */
}
#if defined(__amd64__) && defined(COMPAT_LINUX32)
__packed
#endif
;

struct l_semid_ds {
	struct l_ipc_perm	sem_perm;
	l_time_t		sem_otime;
	l_time_t		sem_ctime;
	l_uintptr_t		sem_base;
	l_uintptr_t		sem_pending;
	l_uintptr_t		sem_pending_last;
	l_uintptr_t		undo;
	l_ushort		sem_nsems;
}
#if defined(__amd64__) && defined(COMPAT_LINUX32)
__packed
#endif
;

struct l_shmid_ds {
	struct l_ipc_perm	shm_perm;
	l_int			shm_segsz;
	l_time_t		shm_atime;
	l_time_t		shm_dtime;
	l_time_t		shm_ctime;
	l_ushort		shm_cpid;
	l_ushort		shm_lpid;
	l_short			shm_nattch;
	l_ushort		private1;
	l_uintptr_t		private2;
	l_uintptr_t		private3;
};

static void
linux_to_bsd_semid_ds(struct l_semid_ds *lsp, struct semid_ds *bsp)
{

	linux_to_bsd_ipc_perm(&lsp->sem_perm, &bsp->sem_perm);
	bsp->sem_otime = lsp->sem_otime;
	bsp->sem_ctime = lsp->sem_ctime;
	bsp->sem_nsems = lsp->sem_nsems;
	bsp->sem_base = PTRIN(lsp->sem_base);
}

static void
bsd_to_linux_semid_ds(struct semid_ds *bsp, struct l_semid_ds *lsp)
{

	bsd_to_linux_ipc_perm(&bsp->sem_perm, &lsp->sem_perm);
	lsp->sem_otime = bsp->sem_otime;
	lsp->sem_ctime = bsp->sem_ctime;
	lsp->sem_nsems = bsp->sem_nsems;
	lsp->sem_base = PTROUT(bsp->sem_base);
}

static void
linux_to_bsd_shmid_ds(struct l_shmid_ds *lsp, struct shmid_ds *bsp)
{

	linux_to_bsd_ipc_perm(&lsp->shm_perm, &bsp->shm_perm);
	bsp->shm_segsz = lsp->shm_segsz;
	bsp->shm_lpid = lsp->shm_lpid;
	bsp->shm_cpid = lsp->shm_cpid;
	bsp->shm_nattch = lsp->shm_nattch;
	bsp->shm_atime = lsp->shm_atime;
	bsp->shm_dtime = lsp->shm_dtime;
	bsp->shm_ctime = lsp->shm_ctime;
}

static void
bsd_to_linux_shmid_ds(struct shmid_ds *bsp, struct l_shmid_ds *lsp)
{

	bsd_to_linux_ipc_perm(&bsp->shm_perm, &lsp->shm_perm);
	if (bsp->shm_segsz > INT_MAX)
		lsp->shm_segsz = INT_MAX;
	else
		lsp->shm_segsz = bsp->shm_segsz;
	lsp->shm_lpid = bsp->shm_lpid;
	lsp->shm_cpid = bsp->shm_cpid;
	if (bsp->shm_nattch > SHRT_MAX)
		lsp->shm_nattch = SHRT_MAX;
	else
		lsp->shm_nattch = bsp->shm_nattch;
	lsp->shm_atime = bsp->shm_atime;
	lsp->shm_dtime = bsp->shm_dtime;
	lsp->shm_ctime = bsp->shm_ctime;
	lsp->private3 = 0;
}

static void
linux_to_bsd_msqid_ds(struct l_msqid_ds *lsp, struct msqid_ds *bsp)
{

	linux_to_bsd_ipc_perm(&lsp->msg_perm, &bsp->msg_perm);
	bsp->msg_cbytes = lsp->msg_cbytes;
	bsp->msg_qnum = lsp->msg_qnum;
	bsp->msg_qbytes = lsp->msg_qbytes;
	bsp->msg_lspid = lsp->msg_lspid;
	bsp->msg_lrpid = lsp->msg_lrpid;
	bsp->msg_stime = lsp->msg_stime;
	bsp->msg_rtime = lsp->msg_rtime;
	bsp->msg_ctime = lsp->msg_ctime;
}

static void
bsd_to_linux_msqid_ds(struct msqid_ds *bsp, struct l_msqid_ds *lsp)
{

	bsd_to_linux_ipc_perm(&bsp->msg_perm, &lsp->msg_perm);
	lsp->msg_cbytes = bsp->msg_cbytes;
	lsp->msg_qnum = bsp->msg_qnum;
	lsp->msg_qbytes = bsp->msg_qbytes;
	lsp->msg_lspid = bsp->msg_lspid;
	lsp->msg_lrpid = bsp->msg_lrpid;
	lsp->msg_stime = bsp->msg_stime;
	lsp->msg_rtime = bsp->msg_rtime;
	lsp->msg_ctime = bsp->msg_ctime;
}

static void
linux_ipc_perm_to_ipc64_perm(struct l_ipc_perm *in, struct l_ipc64_perm *out)
{

	/* XXX: do we really need to do something here? */
	out->key = in->key;
	out->uid = in->uid;
	out->gid = in->gid;
	out->cuid = in->cuid;
	out->cgid = in->cgid;
	out->mode = in->mode;
	out->seq = in->seq;
}

static int
linux_msqid_pullup(l_int ver, struct l_msqid_ds *linux_msqid, caddr_t uaddr)
{
	struct l_msqid64_ds linux_msqid64;
	int error;

	if (ver == LINUX_IPC_64) {
		error = copyin(uaddr, &linux_msqid64, sizeof(linux_msqid64));
		if (error != 0)
			return (error);

		bzero(linux_msqid, sizeof(*linux_msqid));

		linux_msqid->msg_perm.uid = linux_msqid64.msg_perm.uid;
		linux_msqid->msg_perm.gid = linux_msqid64.msg_perm.gid;
		linux_msqid->msg_perm.mode = linux_msqid64.msg_perm.mode;

		if (linux_msqid64.msg_qbytes > USHRT_MAX)
			linux_msqid->msg_lqbytes = linux_msqid64.msg_qbytes;
		else
			linux_msqid->msg_qbytes = linux_msqid64.msg_qbytes;
	} else
		error = copyin(uaddr, linux_msqid, sizeof(*linux_msqid));

	return (error);
}

static int
linux_msqid_pushdown(l_int ver, struct l_msqid_ds *linux_msqid, caddr_t uaddr)
{
	struct l_msqid64_ds linux_msqid64;

	if (ver == LINUX_IPC_64) {
		bzero(&linux_msqid64, sizeof(linux_msqid64));

		linux_ipc_perm_to_ipc64_perm(&linux_msqid->msg_perm,
		    &linux_msqid64.msg_perm);

		linux_msqid64.msg_stime = linux_msqid->msg_stime;
		linux_msqid64.msg_rtime = linux_msqid->msg_rtime;
		linux_msqid64.msg_ctime = linux_msqid->msg_ctime;

		if (linux_msqid->msg_cbytes == 0)
			linux_msqid64.msg_cbytes = linux_msqid->msg_lcbytes;
		else
			linux_msqid64.msg_cbytes = linux_msqid->msg_cbytes;

		linux_msqid64.msg_qnum = linux_msqid->msg_qnum;

		if (linux_msqid->msg_qbytes == 0)
			linux_msqid64.msg_qbytes = linux_msqid->msg_lqbytes;
		else
			linux_msqid64.msg_qbytes = linux_msqid->msg_qbytes;

		linux_msqid64.msg_lspid = linux_msqid->msg_lspid;
		linux_msqid64.msg_lrpid = linux_msqid->msg_lrpid;

		return (copyout(&linux_msqid64, uaddr, sizeof(linux_msqid64)));
	} else
		return (copyout(linux_msqid, uaddr, sizeof(*linux_msqid)));
}

static int
linux_semid_pullup(l_int ver, struct l_semid_ds *linux_semid, caddr_t uaddr)
{
	struct l_semid64_ds linux_semid64;
	int error;

	if (ver == LINUX_IPC_64) {
		error = copyin(uaddr, &linux_semid64, sizeof(linux_semid64));
		if (error != 0)
			return (error);

		bzero(linux_semid, sizeof(*linux_semid));

		linux_semid->sem_perm.uid = linux_semid64.sem_perm.uid;
		linux_semid->sem_perm.gid = linux_semid64.sem_perm.gid;
		linux_semid->sem_perm.mode = linux_semid64.sem_perm.mode;
	} else
		error = copyin(uaddr, linux_semid, sizeof(*linux_semid));

	return (error);
}

static int
linux_semid_pushdown(l_int ver, struct l_semid_ds *linux_semid, caddr_t uaddr)
{
	struct l_semid64_ds linux_semid64;

	if (ver == LINUX_IPC_64) {
		bzero(&linux_semid64, sizeof(linux_semid64));

		linux_ipc_perm_to_ipc64_perm(&linux_semid->sem_perm,
		    &linux_semid64.sem_perm);

		linux_semid64.sem_otime = linux_semid->sem_otime;
		linux_semid64.sem_ctime = linux_semid->sem_ctime;
		linux_semid64.sem_nsems = linux_semid->sem_nsems;

		return (copyout(&linux_semid64, uaddr, sizeof(linux_semid64)));
	} else
		return (copyout(linux_semid, uaddr, sizeof(*linux_semid)));
}

static int
linux_shmid_pullup(l_int ver, struct l_shmid_ds *linux_shmid, caddr_t uaddr)
{
	struct l_shmid64_ds linux_shmid64;
	int error;

	if (ver == LINUX_IPC_64) {
		error = copyin(uaddr, &linux_shmid64, sizeof(linux_shmid64));
		if (error != 0)
			return (error);

		bzero(linux_shmid, sizeof(*linux_shmid));

		linux_shmid->shm_perm.uid = linux_shmid64.shm_perm.uid;
		linux_shmid->shm_perm.gid = linux_shmid64.shm_perm.gid;
		linux_shmid->shm_perm.mode = linux_shmid64.shm_perm.mode;
	} else
		error = copyin(uaddr, linux_shmid, sizeof(*linux_shmid));

	return (error);
}

static int
linux_shmid_pushdown(l_int ver, struct l_shmid_ds *linux_shmid, caddr_t uaddr)
{
	struct l_shmid64_ds linux_shmid64;

	/*
	 * XXX: This is backwards and loses information in shm_nattch
	 * and shm_segsz.  We should probably either expose the BSD
	 * shmid structure directly and convert it to either the
	 * non-64 or 64 variant directly or the code should always
	 * convert to the 64 variant and then truncate values into the
	 * non-64 variant if needed since the 64 variant has more
	 * precision.
	 */
	if (ver == LINUX_IPC_64) {
		bzero(&linux_shmid64, sizeof(linux_shmid64));

		linux_ipc_perm_to_ipc64_perm(&linux_shmid->shm_perm,
		    &linux_shmid64.shm_perm);

		linux_shmid64.shm_segsz = linux_shmid->shm_segsz;
		linux_shmid64.shm_atime = linux_shmid->shm_atime;
		linux_shmid64.shm_dtime = linux_shmid->shm_dtime;
		linux_shmid64.shm_ctime = linux_shmid->shm_ctime;
		linux_shmid64.shm_cpid = linux_shmid->shm_cpid;
		linux_shmid64.shm_lpid = linux_shmid->shm_lpid;
		linux_shmid64.shm_nattch = linux_shmid->shm_nattch;

		return (copyout(&linux_shmid64, uaddr, sizeof(linux_shmid64)));
	} else
		return (copyout(linux_shmid, uaddr, sizeof(*linux_shmid)));
}

static int
linux_shminfo_pushdown(l_int ver, struct l_shminfo *linux_shminfo,
    caddr_t uaddr)
{
	struct l_shminfo64 linux_shminfo64;

	if (ver == LINUX_IPC_64) {
		bzero(&linux_shminfo64, sizeof(linux_shminfo64));

		linux_shminfo64.shmmax = linux_shminfo->shmmax;
		linux_shminfo64.shmmin = linux_shminfo->shmmin;
		linux_shminfo64.shmmni = linux_shminfo->shmmni;
		linux_shminfo64.shmseg = linux_shminfo->shmseg;
		linux_shminfo64.shmall = linux_shminfo->shmall;

		return (copyout(&linux_shminfo64, uaddr,
		    sizeof(linux_shminfo64)));
	} else
		return (copyout(linux_shminfo, uaddr, sizeof(*linux_shminfo)));
}

int
linux_semop(struct thread *td, struct linux_semop_args *args)
{
	struct semop_args /* {
	int	semid;
	struct	sembuf *sops;
	int		nsops;
	} */ bsd_args;

	bsd_args.semid = args->semid;
	bsd_args.sops = PTRIN(args->tsops);
	bsd_args.nsops = args->nsops;
	return (sys_semop(td, &bsd_args));
}

int
linux_semget(struct thread *td, struct linux_semget_args *args)
{
	struct semget_args /* {
	key_t	key;
	int		nsems;
	int		semflg;
	} */ bsd_args;

	if (args->nsems < 0)
		return (EINVAL);
	bsd_args.key = args->key;
	bsd_args.nsems = args->nsems;
	bsd_args.semflg = args->semflg;
	return (sys_semget(td, &bsd_args));
}

int
linux_semctl(struct thread *td, struct linux_semctl_args *args)
{
	struct l_semid_ds linux_semid;
	struct l_seminfo linux_seminfo;
	struct semid_ds semid;
	union semun semun;
	register_t rval;
	int cmd, error;

	switch (args->cmd & ~LINUX_IPC_64) {
	case LINUX_IPC_RMID:
		cmd = IPC_RMID;
		break;
	case LINUX_GETNCNT:
		cmd = GETNCNT;
		break;
	case LINUX_GETPID:
		cmd = GETPID;
		break;
	case LINUX_GETVAL:
		cmd = GETVAL;
		break;
	case LINUX_GETZCNT:
		cmd = GETZCNT;
		break;
	case LINUX_SETVAL:
		cmd = SETVAL;
		semun.val = args->arg.val;
		break;
	case LINUX_IPC_SET:
		cmd = IPC_SET;
		error = linux_semid_pullup(args->cmd & LINUX_IPC_64,
		    &linux_semid, PTRIN(args->arg.buf));
		if (error)
			return (error);
		linux_to_bsd_semid_ds(&linux_semid, &semid);
		semun.buf = &semid;
		return (kern_semctl(td, args->semid, args->semnum, cmd, &semun,
		    td->td_retval));
	case LINUX_IPC_STAT:
	case LINUX_SEM_STAT:
		if ((args->cmd & ~LINUX_IPC_64) == LINUX_IPC_STAT)
			cmd = IPC_STAT;
		else
			cmd = SEM_STAT;
		semun.buf = &semid;
		error = kern_semctl(td, args->semid, args->semnum, cmd, &semun,
		    &rval);
		if (error)
			return (error);
		bsd_to_linux_semid_ds(&semid, &linux_semid);
		error = linux_semid_pushdown(args->cmd & LINUX_IPC_64,
		    &linux_semid, PTRIN(args->arg.buf));
		if (error == 0)
			td->td_retval[0] = (cmd == SEM_STAT) ? rval : 0;
		return (error);
	case LINUX_IPC_INFO:
	case LINUX_SEM_INFO:
		bcopy(&seminfo, &linux_seminfo.semmni, sizeof(linux_seminfo) -
		    sizeof(linux_seminfo.semmap) );
		/*
		 * Linux does not use the semmap field but populates it with
		 * the defined value from SEMMAP, which really is redefined to
		 * SEMMNS, which they define as SEMMNI * SEMMSL.  Try to
		 * simulate this returning our dynamic semmns value.
		 */
		linux_seminfo.semmap = linux_seminfo.semmns;
/* XXX BSD equivalent?
#define used_semids 10
#define used_sems 10
		linux_seminfo.semusz = used_semids;
		linux_seminfo.semaem = used_sems;
*/
		error = copyout(&linux_seminfo,
		    PTRIN(args->arg.buf), sizeof(linux_seminfo));
		if (error)
			return (error);
		td->td_retval[0] = seminfo.semmni;
		return (0);			/* No need for __semctl call */
	case LINUX_GETALL:
		cmd = GETALL;
		semun.val = args->arg.val;
		break;
	case LINUX_SETALL:
		cmd = SETALL;
		semun.val = args->arg.val;
		break;
	default:
		linux_msg(td, "ipc type %d is not implemented",
		  args->cmd & ~LINUX_IPC_64);
		return (EINVAL);
	}
	return (kern_semctl(td, args->semid, args->semnum, cmd, &semun,
	    td->td_retval));
}

int
linux_msgsnd(struct thread *td, struct linux_msgsnd_args *args)
{
	const void *msgp;
	long mtype;
	l_long lmtype;
	int error;

	if ((l_long)args->msgsz < 0 || args->msgsz > (l_long)msginfo.msgmax)
		return (EINVAL);
	msgp = PTRIN(args->msgp);
	if ((error = copyin(msgp, &lmtype, sizeof(lmtype))) != 0)
		return (error);
	mtype = (long)lmtype;
	return (kern_msgsnd(td, args->msqid,
	    (const char *)msgp + sizeof(lmtype),
	    args->msgsz, args->msgflg, mtype));
}

int
linux_msgrcv(struct thread *td, struct linux_msgrcv_args *args)
{
	void *msgp;
	long mtype;
	l_long lmtype;
	int error;

	if ((l_long)args->msgsz < 0 || args->msgsz > (l_long)msginfo.msgmax)
		return (EINVAL);
	msgp = PTRIN(args->msgp);
	if ((error = kern_msgrcv(td, args->msqid,
	    (char *)msgp + sizeof(lmtype), args->msgsz,
	    args->msgtyp, args->msgflg, &mtype)) != 0)
		return (error);
	lmtype = (l_long)mtype;
	return (copyout(&lmtype, msgp, sizeof(lmtype)));
}

int
linux_msgget(struct thread *td, struct linux_msgget_args *args)
{
	struct msgget_args /* {
		key_t	key;
		int	msgflg;
	} */ bsd_args;

	bsd_args.key = args->key;
	bsd_args.msgflg = args->msgflg;
	return (sys_msgget(td, &bsd_args));
}

int
linux_msgctl(struct thread *td, struct linux_msgctl_args *args)
{
	int error, bsd_cmd;
	struct l_msqid_ds linux_msqid;
	struct msqid_ds bsd_msqid;

	bsd_cmd = args->cmd & ~LINUX_IPC_64;
	switch (bsd_cmd) {
	case LINUX_IPC_INFO:
	case LINUX_MSG_INFO: {
		struct l_msginfo linux_msginfo;

		/*
		 * XXX MSG_INFO uses the same data structure but returns different
		 * dynamic counters in msgpool, msgmap, and msgtql fields.
		 */
		linux_msginfo.msgpool = (long)msginfo.msgmni *
		    (long)msginfo.msgmnb / 1024L;	/* XXX MSG_INFO. */
		linux_msginfo.msgmap = msginfo.msgmnb;	/* XXX MSG_INFO. */
		linux_msginfo.msgmax = msginfo.msgmax;
		linux_msginfo.msgmnb = msginfo.msgmnb;
		linux_msginfo.msgmni = msginfo.msgmni;
		linux_msginfo.msgssz = msginfo.msgssz;
		linux_msginfo.msgtql = msginfo.msgtql;	/* XXX MSG_INFO. */
		linux_msginfo.msgseg = msginfo.msgseg;
		error = copyout(&linux_msginfo, PTRIN(args->buf),
		    sizeof(linux_msginfo));
		if (error == 0)
		    td->td_retval[0] = msginfo.msgmni;	/* XXX */

		return (error);
	}

	/* 
	 * TODO: implement this 
	 * case LINUX_MSG_STAT:
	 */
	case LINUX_IPC_STAT:
		/* NOTHING */
		break;

	case LINUX_IPC_SET:
		error = linux_msqid_pullup(args->cmd & LINUX_IPC_64,
		    &linux_msqid, PTRIN(args->buf));
		if (error)
			return (error);
		linux_to_bsd_msqid_ds(&linux_msqid, &bsd_msqid);
		break;

	case LINUX_IPC_RMID:
		/* NOTHING */
		break;

	default:
		return (EINVAL);
		break;
	}

	error = kern_msgctl(td, args->msqid, bsd_cmd, &bsd_msqid);
	if (error != 0)
		if (bsd_cmd != LINUX_IPC_RMID || error != EINVAL)
			return (error);

	if (bsd_cmd == LINUX_IPC_STAT) {
		bsd_to_linux_msqid_ds(&bsd_msqid, &linux_msqid);
		return (linux_msqid_pushdown(args->cmd & LINUX_IPC_64,
		    &linux_msqid, PTRIN(args->buf)));
	}

	return (0);
}

int
linux_shmat(struct thread *td, struct linux_shmat_args *args)
{
	struct shmat_args /* {
		int shmid;
		void *shmaddr;
		int shmflg;
	} */ bsd_args;
	int error;
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
	l_uintptr_t addr;
#endif

	bsd_args.shmid = args->shmid;
	bsd_args.shmaddr = PTRIN(args->shmaddr);
	bsd_args.shmflg = args->shmflg;
	if ((error = sys_shmat(td, &bsd_args)))
		return (error);
#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
	addr = td->td_retval[0];
	if ((error = copyout(&addr, PTRIN(args->raddr), sizeof(addr))))
		return (error);
	td->td_retval[0] = 0;
#endif
	return (0);
}

int
linux_shmdt(struct thread *td, struct linux_shmdt_args *args)
{
	struct shmdt_args /* {
		void *shmaddr;
	} */ bsd_args;

	bsd_args.shmaddr = PTRIN(args->shmaddr);
	return (sys_shmdt(td, &bsd_args));
}

int
linux_shmget(struct thread *td, struct linux_shmget_args *args)
{
	struct shmget_args /* {
		key_t key;
		int size;
		int shmflg;
	} */ bsd_args;

	bsd_args.key = args->key;
	bsd_args.size = args->size;
	bsd_args.shmflg = args->shmflg;
	return (sys_shmget(td, &bsd_args));
}

int
linux_shmctl(struct thread *td, struct linux_shmctl_args *args)
{
	struct l_shmid_ds linux_shmid;
	struct l_shminfo linux_shminfo;
	struct l_shm_info linux_shm_info;
	struct shmid_ds bsd_shmid;
	int error;

	switch (args->cmd & ~LINUX_IPC_64) {

	case LINUX_IPC_INFO: {
		struct shminfo bsd_shminfo;

		/* Perform shmctl wanting removed segments lookup */
		error = kern_shmctl(td, args->shmid, IPC_INFO,
		    (void *)&bsd_shminfo, NULL);
		if (error)
			return (error);

		bsd_to_linux_shminfo(&bsd_shminfo, &linux_shminfo);

		return (linux_shminfo_pushdown(args->cmd & LINUX_IPC_64,
		    &linux_shminfo, PTRIN(args->buf)));
	}

	case LINUX_SHM_INFO: {
		struct shm_info bsd_shm_info;

		/* Perform shmctl wanting removed segments lookup */
		error = kern_shmctl(td, args->shmid, SHM_INFO,
		    (void *)&bsd_shm_info, NULL);
		if (error)
			return (error);

		bsd_to_linux_shm_info(&bsd_shm_info, &linux_shm_info);

		return (copyout(&linux_shm_info, PTRIN(args->buf),
		    sizeof(struct l_shm_info)));
	}

	case LINUX_IPC_STAT:
		/* Perform shmctl wanting removed segments lookup */
		error = kern_shmctl(td, args->shmid, IPC_STAT,
		    (void *)&bsd_shmid, NULL);
		if (error)
			return (error);
		
		bsd_to_linux_shmid_ds(&bsd_shmid, &linux_shmid);

		return (linux_shmid_pushdown(args->cmd & LINUX_IPC_64,
		    &linux_shmid, PTRIN(args->buf)));

	case LINUX_SHM_STAT:
		/* Perform shmctl wanting removed segments lookup */
		error = kern_shmctl(td, args->shmid, IPC_STAT,
		    (void *)&bsd_shmid, NULL);
		if (error)
			return (error);
		
		bsd_to_linux_shmid_ds(&bsd_shmid, &linux_shmid);

		return (linux_shmid_pushdown(args->cmd & LINUX_IPC_64,
		    &linux_shmid, PTRIN(args->buf)));

	case LINUX_IPC_SET:
		error = linux_shmid_pullup(args->cmd & LINUX_IPC_64,
		    &linux_shmid, PTRIN(args->buf));
		if (error)
			return (error);

		linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);

		/* Perform shmctl wanting removed segments lookup */
		return (kern_shmctl(td, args->shmid, IPC_SET,
		    (void *)&bsd_shmid, NULL));

	case LINUX_IPC_RMID: {
		void *buf;

		if (args->buf == 0)
			buf = NULL;
		else {
			error = linux_shmid_pullup(args->cmd & LINUX_IPC_64,
			    &linux_shmid, PTRIN(args->buf));
			if (error)
				return (error);
			linux_to_bsd_shmid_ds(&linux_shmid, &bsd_shmid);
			buf = (void *)&bsd_shmid;
		}
		return (kern_shmctl(td, args->shmid, IPC_RMID, buf, NULL));
	}

	case LINUX_SHM_LOCK:
		/* FALLTHROUGH */
	case LINUX_SHM_UNLOCK:
		/* FALLTHROUGH */
	default:
		linux_msg(td, "ipc type %d not implemented",
		    args->cmd & ~LINUX_IPC_64);
		return (EINVAL);
	}
}

MODULE_DEPEND(linux, sysvmsg, 1, 1, 1);
MODULE_DEPEND(linux, sysvsem, 1, 1, 1);
MODULE_DEPEND(linux, sysvshm, 1, 1, 1);
