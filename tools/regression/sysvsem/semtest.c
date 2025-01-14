/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Obtained from: $NetBSD: semtest.c,v 1.4 2002/07/20 08:36:25 grant Exp $
 * $FreeBSD: head/tools/regression/sysvsem/semtest.c 295385 2016-02-07 22:12:39Z jilles $
 */

/*
 * Test the SVID-compatible Semaphore facility.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int	main (int, char *[]);
void	print_semid_ds (struct semid_ds *, mode_t);
void	sigsys_handler (int);
void	sigchld_handler(int);
void	cleanup (void);
void	waiter (void);
void	usage (void);

int	sender_semid = -1;
pid_t	child_pid;
int	child_count;
int	signal_was_sigchld;

key_t	semkey;

/*
 * This is the original semun union used by the sysvsem utility.
 * It is deliberately kept here under #if 0'ed condition for future
 * reference. PLEASE DO NOT REMOVE.  The {SET,GET}ALL in FreeBSD
 * are signed values, so the default version in sys/sem.h suffices.
 */
#if 0
union semun {
	int	val;		/* value for SETVAL */
	struct	semid_ds *buf;	/* buffer for IPC_{STAT,SET} */
	u_short	*array;		/* array for GETALL & SETALL */
};
#endif

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	union semun sun;
	struct semid_ds s_ds;
	sigset_t sigmask;
	int i;

	if (argc != 2)
		usage();

	/*
	 * Install a SIGSYS handler so that we can exit gracefully if
	 * System V Semaphore support isn't in the kernel.
	 */
	sa.sa_handler = sigsys_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGSYS, &sa, NULL) == -1)
		err(1, "sigaction SIGSYS");

	/*
	 * Install and SIGCHLD handler to deal with all possible exit
	 * conditions of the receiver.
	 */
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		err(1, "sigaction SIGCHLD");

	semkey = ftok(argv[1], 4160);

	/*
	 * Initialize child_pid to ourselves to that the cleanup function
	 * works before we create the receiver.
	 */
	child_pid = getpid();

	/*
	 * Make sure that when the sender exits, the message queue is
	 * removed.
	 */
	if (atexit(cleanup) == -1)
		err(1, "atexit");

	if ((sender_semid = semget(semkey, 1, IPC_CREAT | 0640)) == -1)
		err(1, "semget");

	
	sun.buf = &s_ds;
	if (semctl(sender_semid, 0, IPC_STAT, sun) == -1)
		err(1, "semctl IPC_STAT");

	print_semid_ds(&s_ds, 0640);

	s_ds.sem_perm.mode = (s_ds.sem_perm.mode & ~0777) | 0600;

	sun.buf = &s_ds;
	if (semctl(sender_semid, 0, IPC_SET, sun) == -1)
		err(1, "semctl IPC_SET");

	memset(&s_ds, 0, sizeof(s_ds));

	sun.buf = &s_ds;
	if (semctl(sender_semid, 0, IPC_STAT, sun) == -1)
		err(1, "semctl IPC_STAT");

	if ((s_ds.sem_perm.mode & 0777) != 0600)
		err(1, "IPC_SET of mode didn't hold");

	print_semid_ds(&s_ds, 0600);

	errno = 0;
	if (semget(semkey, 1, IPC_CREAT | IPC_EXCL | 0600) != -1 ||
	    errno != EEXIST)
		err(1, "semget IPC_EXCL 1 did not fail with [EEXIST]");
	errno = 0;
	if (semget(semkey, 2, IPC_CREAT | IPC_EXCL | 0600) != -1 ||
	    errno != EEXIST)
		err(1, "semget IPC_EXCL 2 did not fail with [EEXIST]");

	for (child_count = 0; child_count < 5; child_count++) {
		switch ((child_pid = fork())) {
		case -1:
			err(1, "fork");
			/* NOTREACHED */

		case 0:
			waiter();
			break;

		default:
			break;
		}
	}

	/*
	 * Wait for all of the waiters to be attempting to acquire the
	 * semaphore.
	 */
	for (;;) {
		i = semctl(sender_semid, 0, GETNCNT);
		if (i == -1)
			err(1, "semctl GETNCNT");
		if (i == 5)
			break;
	}

	/*
	 * Now set the thundering herd in motion by initializing the
	 * semaphore to the value 1.
	 */
	sun.val = 1;
	if (semctl(sender_semid, 0, SETVAL, sun) == -1)
		err(1, "sender: semctl SETVAL to 1");

	/*
	 * Suspend forever; when we get SIGCHLD, the handler will exit.
	 */
	sigemptyset(&sigmask);
	for (;;) {
		(void) sigsuspend(&sigmask);
		if (signal_was_sigchld)
			signal_was_sigchld = 0;
		else
			break;
	}

	/*
	 * ...and any other signal is an unexpected error.
	 */
	errx(1, "sender: received unexpected signal");
}

void
sigsys_handler(int signo)
{

	errx(1, "System V Semaphore support is not present in the kernel");
}

void
sigchld_handler(int signo)
{
	union semun sun;
	struct semid_ds s_ds;
	int cstatus;

	/*
	 * Reap the child; if it exited successfully, then we're on the
	 * right track!
	 */
	if (wait(&cstatus) == -1)
		err(1, "wait");

	if (WIFEXITED(cstatus) == 0)
		errx(1, "receiver exited abnormally");

	if (WEXITSTATUS(cstatus) != 0)
		errx(1, "receiver exited with status %d",
		    WEXITSTATUS(cstatus));

	/*
	 * If we get here, the child has exited normally, and we should
	 * decrement the child count.  If the child_count reaches 0, we
	 * should exit.
	 */

	sun.buf = &s_ds;
	if (semctl(sender_semid, 0, IPC_STAT, sun) == -1)
		err(1, "semctl IPC_STAT");

	print_semid_ds(&s_ds, 0600);

	if (--child_count != 0) {
		signal_was_sigchld = 1;
		return;
	}

	exit(0);
}

void
cleanup()
{

	/*
	 * If we're the sender, and it exists, remove the message queue.
	 */
	if (child_pid != 0 && sender_semid != -1) {
		if (semctl(sender_semid, 0, IPC_RMID) == -1)
			warn("semctl IPC_RMID");
	}
}

void
print_semid_ds(struct semid_ds *sp, mode_t mode)
{
	uid_t uid = geteuid();
	gid_t gid = getegid();

	printf("PERM: uid %d, gid %d, cuid %d, cgid %d, mode 0%o\n",
	    sp->sem_perm.uid, sp->sem_perm.gid,
	    sp->sem_perm.cuid, sp->sem_perm.cgid,
	    sp->sem_perm.mode & 0777);

	printf("nsems %u\n", sp->sem_nsems);

	printf("otime: %s", ctime(&sp->sem_otime));
	printf("ctime: %s", ctime(&sp->sem_ctime));

	/*
	 * Sanity check a few things.
	 */

	if (sp->sem_perm.uid != uid || sp->sem_perm.cuid != uid)
		errx(1, "uid mismatch");

	if (sp->sem_perm.gid != gid || sp->sem_perm.cgid != gid)
		errx(1, "gid mismatch");

	if ((sp->sem_perm.mode & 0777) != mode)
		errx(1, "mode mismatch %o != %o",
		    (sp->sem_perm.mode & 0777), mode);
}

void
usage()
{

	fprintf(stderr, "usage: %s keypath\n", getprogname());
	exit(1);
}

void
waiter()
{
	struct sembuf s;
	int semid;

	if ((semid = semget(semkey, 1, 0)) == -1)
		err(1, "waiter: semget");

	/*
	 * Attempt to acquire the semaphore.
	 */
	s.sem_num = 0;
	s.sem_op = -1;
	s.sem_flg = SEM_UNDO;

	if (semop(semid, &s, 1) == -1)
		err(1, "waiter: semop -1");

	printf("WOO!  GOT THE SEMAPHORE!\n");
	sleep(1);

	/*
	 * Release the semaphore and exit.
	 */
	s.sem_num = 0;
	s.sem_op = 1;
	s.sem_flg = SEM_UNDO;

	if (semop(semid, &s, 1) == -1)
		err(1, "waiter: semop +1");

	exit(0);
}
