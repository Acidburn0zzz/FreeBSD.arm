/*-
 * Copyright (c) 2000 Doug Rabson
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
__FBSDID("$FreeBSD: stable/11/sys/kern/subr_taskqueue.c 302372 2016-07-06 14:09:49Z nwhitehorn $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

static MALLOC_DEFINE(M_TASKQUEUE, "taskqueue", "Task Queues");
static void	*taskqueue_giant_ih;
static void	*taskqueue_ih;
static void	 taskqueue_fast_enqueue(void *);
static void	 taskqueue_swi_enqueue(void *);
static void	 taskqueue_swi_giant_enqueue(void *);

struct taskqueue_busy {
	struct task	*tb_running;
	TAILQ_ENTRY(taskqueue_busy) tb_link;
};

struct task * const TB_DRAIN_WAITER = (struct task *)0x1;

struct taskqueue {
	STAILQ_HEAD(, task)	tq_queue;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	char			*tq_name;
	TAILQ_HEAD(, taskqueue_busy) tq_active;
	struct mtx		tq_mutex;
	struct thread		**tq_threads;
	int			tq_tcount;
	int			tq_spin;
	int			tq_flags;
	int			tq_callouts;
	taskqueue_callback_fn	tq_callbacks[TASKQUEUE_NUM_CALLBACKS];
	void			*tq_cb_contexts[TASKQUEUE_NUM_CALLBACKS];
};

#define	TQ_FLAGS_ACTIVE		(1 << 0)
#define	TQ_FLAGS_BLOCKED	(1 << 1)
#define	TQ_FLAGS_UNLOCKED_ENQUEUE	(1 << 2)

#define	DT_CALLOUT_ARMED	(1 << 0)

#define	TQ_LOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_lock_spin(&(tq)->tq_mutex);			\
		else							\
			mtx_lock(&(tq)->tq_mutex);			\
	} while (0)
#define	TQ_ASSERT_LOCKED(tq)	mtx_assert(&(tq)->tq_mutex, MA_OWNED)

#define	TQ_UNLOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_unlock_spin(&(tq)->tq_mutex);		\
		else							\
			mtx_unlock(&(tq)->tq_mutex);			\
	} while (0)
#define	TQ_ASSERT_UNLOCKED(tq)	mtx_assert(&(tq)->tq_mutex, MA_NOTOWNED)

void
_timeout_task_init(struct taskqueue *queue, struct timeout_task *timeout_task,
    int priority, task_fn_t func, void *context)
{

	TASK_INIT(&timeout_task->t, priority, func, context);
	callout_init_mtx(&timeout_task->c, &queue->tq_mutex,
	    CALLOUT_RETURNUNLOCKED);
	timeout_task->q = queue;
	timeout_task->f = 0;
}

static __inline int
TQ_SLEEP(struct taskqueue *tq, void *p, struct mtx *m, int pri, const char *wm,
    int t)
{
	if (tq->tq_spin)
		return (msleep_spin(p, m, wm, t));
	return (msleep(p, m, pri, wm, t));
}

static struct taskqueue *
_taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context,
		 int mtxflags, const char *mtxname __unused)
{
	struct taskqueue *queue;
	char *tq_name;

	tq_name = malloc(TASKQUEUE_NAMELEN, M_TASKQUEUE, mflags | M_ZERO);
	if (tq_name == NULL)
		return (NULL);

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (queue == NULL) {
		free(tq_name, M_TASKQUEUE);
		return (NULL);
	}

	snprintf(tq_name, TASKQUEUE_NAMELEN, "%s", (name) ? name : "taskqueue");

	STAILQ_INIT(&queue->tq_queue);
	TAILQ_INIT(&queue->tq_active);
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_name = tq_name;
	queue->tq_spin = (mtxflags & MTX_SPIN) != 0;
	queue->tq_flags |= TQ_FLAGS_ACTIVE;
	if (enqueue == taskqueue_fast_enqueue ||
	    enqueue == taskqueue_swi_enqueue ||
	    enqueue == taskqueue_swi_giant_enqueue ||
	    enqueue == taskqueue_thread_enqueue)
		queue->tq_flags |= TQ_FLAGS_UNLOCKED_ENQUEUE;
	mtx_init(&queue->tq_mutex, tq_name, NULL, mtxflags);

	return (queue);
}

struct taskqueue *
taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context)
{

	return _taskqueue_create(name, mflags, enqueue, context,
			MTX_DEF, name);
}

void
taskqueue_set_callback(struct taskqueue *queue,
    enum taskqueue_callback_type cb_type, taskqueue_callback_fn callback,
    void *context)
{

	KASSERT(((cb_type >= TASKQUEUE_CALLBACK_TYPE_MIN) &&
	    (cb_type <= TASKQUEUE_CALLBACK_TYPE_MAX)),
	    ("Callback type %d not valid, must be %d-%d", cb_type,
	    TASKQUEUE_CALLBACK_TYPE_MIN, TASKQUEUE_CALLBACK_TYPE_MAX));
	KASSERT((queue->tq_callbacks[cb_type] == NULL),
	    ("Re-initialization of taskqueue callback?"));

	queue->tq_callbacks[cb_type] = callback;
	queue->tq_cb_contexts[cb_type] = context;
}

/*
 * Signal a taskqueue thread to terminate.
 */
static void
taskqueue_terminate(struct thread **pp, struct taskqueue *tq)
{

	while (tq->tq_tcount > 0 || tq->tq_callouts > 0) {
		wakeup(tq);
		TQ_SLEEP(tq, pp, &tq->tq_mutex, PWAIT, "taskqueue_destroy", 0);
	}
}

void
taskqueue_free(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_ACTIVE;
	taskqueue_terminate(queue->tq_threads, queue);
	KASSERT(TAILQ_EMPTY(&queue->tq_active), ("Tasks still running?"));
	KASSERT(queue->tq_callouts == 0, ("Armed timeout tasks"));
	mtx_destroy(&queue->tq_mutex);
	free(queue->tq_threads, M_TASKQUEUE);
	free(queue->tq_name, M_TASKQUEUE);
	free(queue, M_TASKQUEUE);
}

static int
taskqueue_enqueue_locked(struct taskqueue *queue, struct task *task)
{
	struct task *ins;
	struct task *prev;

	KASSERT(task->ta_func != NULL, ("enqueueing task with NULL func"));
	/*
	 * Count multiple enqueues.
	 */
	if (task->ta_pending) {
		if (task->ta_pending < USHRT_MAX)
			task->ta_pending++;
		TQ_UNLOCK(queue);
		return (0);
	}

	/*
	 * Optimise the case when all tasks have the same priority.
	 */
	prev = STAILQ_LAST(&queue->tq_queue, task, ta_link);
	if (!prev || prev->ta_priority >= task->ta_priority) {
		STAILQ_INSERT_TAIL(&queue->tq_queue, task, ta_link);
	} else {
		prev = NULL;
		for (ins = STAILQ_FIRST(&queue->tq_queue); ins;
		     prev = ins, ins = STAILQ_NEXT(ins, ta_link))
			if (ins->ta_priority < task->ta_priority)
				break;

		if (prev)
			STAILQ_INSERT_AFTER(&queue->tq_queue, prev, task, ta_link);
		else
			STAILQ_INSERT_HEAD(&queue->tq_queue, task, ta_link);
	}

	task->ta_pending = 1;
	if ((queue->tq_flags & TQ_FLAGS_UNLOCKED_ENQUEUE) != 0)
		TQ_UNLOCK(queue);
	if ((queue->tq_flags & TQ_FLAGS_BLOCKED) == 0)
		queue->tq_enqueue(queue->tq_context);
	if ((queue->tq_flags & TQ_FLAGS_UNLOCKED_ENQUEUE) == 0)
		TQ_UNLOCK(queue);

	/* Return with lock released. */
	return (0);
}

int
grouptaskqueue_enqueue(struct taskqueue *queue, struct task *task)
{
	TQ_LOCK(queue);
	if (task->ta_pending) {
		TQ_UNLOCK(queue);
		return (0);
	}
	STAILQ_INSERT_TAIL(&queue->tq_queue, task, ta_link);
	task->ta_pending = 1;
	TQ_UNLOCK(queue);
	if ((queue->tq_flags & TQ_FLAGS_BLOCKED) == 0)
		queue->tq_enqueue(queue->tq_context);
	return (0);
}

int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{
	int res;

	TQ_LOCK(queue);
	res = taskqueue_enqueue_locked(queue, task);
	/* The lock is released inside. */

	return (res);
}

static void
taskqueue_timeout_func(void *arg)
{
	struct taskqueue *queue;
	struct timeout_task *timeout_task;

	timeout_task = arg;
	queue = timeout_task->q;
	KASSERT((timeout_task->f & DT_CALLOUT_ARMED) != 0, ("Stray timeout"));
	timeout_task->f &= ~DT_CALLOUT_ARMED;
	queue->tq_callouts--;
	taskqueue_enqueue_locked(timeout_task->q, &timeout_task->t);
	/* The lock is released inside. */
}

int
taskqueue_enqueue_timeout(struct taskqueue *queue,
    struct timeout_task *timeout_task, int ticks)
{
	int res;

	TQ_LOCK(queue);
	KASSERT(timeout_task->q == NULL || timeout_task->q == queue,
	    ("Migrated queue"));
	KASSERT(!queue->tq_spin, ("Timeout for spin-queue"));
	timeout_task->q = queue;
	res = timeout_task->t.ta_pending;
	if (ticks == 0) {
		taskqueue_enqueue_locked(queue, &timeout_task->t);
		/* The lock is released inside. */
	} else {
		if ((timeout_task->f & DT_CALLOUT_ARMED) != 0) {
			res++;
		} else {
			queue->tq_callouts++;
			timeout_task->f |= DT_CALLOUT_ARMED;
			if (ticks < 0)
				ticks = -ticks; /* Ignore overflow. */
		}
		if (ticks > 0) {
			callout_reset(&timeout_task->c, ticks,
			    taskqueue_timeout_func, timeout_task);
		}
		TQ_UNLOCK(queue);
	}
	return (res);
}

static void
taskqueue_task_nop_fn(void *context, int pending)
{
}

/*
 * Block until all currently queued tasks in this taskqueue
 * have begun execution.  Tasks queued during execution of
 * this function are ignored.
 */
static void
taskqueue_drain_tq_queue(struct taskqueue *queue)
{
	struct task t_barrier;

	if (STAILQ_EMPTY(&queue->tq_queue))
		return;

	/*
	 * Enqueue our barrier after all current tasks, but with
	 * the highest priority so that newly queued tasks cannot
	 * pass it.  Because of the high priority, we can not use
	 * taskqueue_enqueue_locked directly (which drops the lock
	 * anyway) so just insert it at tail while we have the
	 * queue lock.
	 */
	TASK_INIT(&t_barrier, USHRT_MAX, taskqueue_task_nop_fn, &t_barrier);
	STAILQ_INSERT_TAIL(&queue->tq_queue, &t_barrier, ta_link);
	t_barrier.ta_pending = 1;

	/*
	 * Once the barrier has executed, all previously queued tasks
	 * have completed or are currently executing.
	 */
	while (t_barrier.ta_pending != 0)
		TQ_SLEEP(queue, &t_barrier, &queue->tq_mutex, PWAIT, "-", 0);
}

/*
 * Block until all currently executing tasks for this taskqueue
 * complete.  Tasks that begin execution during the execution
 * of this function are ignored.
 */
static void
taskqueue_drain_tq_active(struct taskqueue *queue)
{
	struct taskqueue_busy tb_marker, *tb_first;

	if (TAILQ_EMPTY(&queue->tq_active))
		return;

	/* Block taskq_terminate().*/
	queue->tq_callouts++;

	/*
	 * Wait for all currently executing taskqueue threads
	 * to go idle.
	 */
	tb_marker.tb_running = TB_DRAIN_WAITER;
	TAILQ_INSERT_TAIL(&queue->tq_active, &tb_marker, tb_link);
	while (TAILQ_FIRST(&queue->tq_active) != &tb_marker)
		TQ_SLEEP(queue, &tb_marker, &queue->tq_mutex, PWAIT, "-", 0);
	TAILQ_REMOVE(&queue->tq_active, &tb_marker, tb_link);

	/*
	 * Wakeup any other drain waiter that happened to queue up
	 * without any intervening active thread.
	 */
	tb_first = TAILQ_FIRST(&queue->tq_active);
	if (tb_first != NULL && tb_first->tb_running == TB_DRAIN_WAITER)
		wakeup(tb_first);

	/* Release taskqueue_terminate(). */
	queue->tq_callouts--;
	if ((queue->tq_flags & TQ_FLAGS_ACTIVE) == 0)
		wakeup_one(queue->tq_threads);
}

void
taskqueue_block(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags |= TQ_FLAGS_BLOCKED;
	TQ_UNLOCK(queue);
}

void
taskqueue_unblock(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_BLOCKED;
	if (!STAILQ_EMPTY(&queue->tq_queue))
		queue->tq_enqueue(queue->tq_context);
	TQ_UNLOCK(queue);
}

static void
taskqueue_run_locked(struct taskqueue *queue)
{
	struct taskqueue_busy tb;
	struct taskqueue_busy *tb_first;
	struct task *task;
	int pending;

	KASSERT(queue != NULL, ("tq is NULL"));
	TQ_ASSERT_LOCKED(queue);
	tb.tb_running = NULL;

	while (STAILQ_FIRST(&queue->tq_queue)) {
		TAILQ_INSERT_TAIL(&queue->tq_active, &tb, tb_link);

		/*
		 * Carefully remove the first task from the queue and
		 * zero its pending count.
		 */
		task = STAILQ_FIRST(&queue->tq_queue);
		KASSERT(task != NULL, ("task is NULL"));
		STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
		pending = task->ta_pending;
		task->ta_pending = 0;
		tb.tb_running = task;
		TQ_UNLOCK(queue);

		KASSERT(task->ta_func != NULL, ("task->ta_func is NULL"));
		task->ta_func(task->ta_context, pending);

		TQ_LOCK(queue);
		tb.tb_running = NULL;
		wakeup(task);

		TAILQ_REMOVE(&queue->tq_active, &tb, tb_link);
		tb_first = TAILQ_FIRST(&queue->tq_active);
		if (tb_first != NULL &&
		    tb_first->tb_running == TB_DRAIN_WAITER)
			wakeup(tb_first);
	}
}

void
taskqueue_run(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	taskqueue_run_locked(queue);
	TQ_UNLOCK(queue);
}

static int
task_is_running(struct taskqueue *queue, struct task *task)
{
	struct taskqueue_busy *tb;

	TQ_ASSERT_LOCKED(queue);
	TAILQ_FOREACH(tb, &queue->tq_active, tb_link) {
		if (tb->tb_running == task)
			return (1);
	}
	return (0);
}

static int
taskqueue_cancel_locked(struct taskqueue *queue, struct task *task,
    u_int *pendp)
{

	if (task->ta_pending > 0)
		STAILQ_REMOVE(&queue->tq_queue, task, task, ta_link);
	if (pendp != NULL)
		*pendp = task->ta_pending;
	task->ta_pending = 0;
	return (task_is_running(queue, task) ? EBUSY : 0);
}

int
taskqueue_cancel(struct taskqueue *queue, struct task *task, u_int *pendp)
{
	int error;

	TQ_LOCK(queue);
	error = taskqueue_cancel_locked(queue, task, pendp);
	TQ_UNLOCK(queue);

	return (error);
}

int
taskqueue_cancel_timeout(struct taskqueue *queue,
    struct timeout_task *timeout_task, u_int *pendp)
{
	u_int pending, pending1;
	int error;

	TQ_LOCK(queue);
	pending = !!(callout_stop(&timeout_task->c) > 0);
	error = taskqueue_cancel_locked(queue, &timeout_task->t, &pending1);
	if ((timeout_task->f & DT_CALLOUT_ARMED) != 0) {
		timeout_task->f &= ~DT_CALLOUT_ARMED;
		queue->tq_callouts--;
	}
	TQ_UNLOCK(queue);

	if (pendp != NULL)
		*pendp = pending + pending1;
	return (error);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{

	if (!queue->tq_spin)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

	TQ_LOCK(queue);
	while (task->ta_pending != 0 || task_is_running(queue, task))
		TQ_SLEEP(queue, task, &queue->tq_mutex, PWAIT, "-", 0);
	TQ_UNLOCK(queue);
}

void
taskqueue_drain_all(struct taskqueue *queue)
{

	if (!queue->tq_spin)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

	TQ_LOCK(queue);
	taskqueue_drain_tq_queue(queue);
	taskqueue_drain_tq_active(queue);
	TQ_UNLOCK(queue);
}

void
taskqueue_drain_timeout(struct taskqueue *queue,
    struct timeout_task *timeout_task)
{

	callout_drain(&timeout_task->c);
	taskqueue_drain(queue, &timeout_task->t);
}

static void
taskqueue_swi_enqueue(void *context)
{
	swi_sched(taskqueue_ih, 0);
}

static void
taskqueue_swi_run(void *dummy)
{
	taskqueue_run(taskqueue_swi);
}

static void
taskqueue_swi_giant_enqueue(void *context)
{
	swi_sched(taskqueue_giant_ih, 0);
}

static void
taskqueue_swi_giant_run(void *dummy)
{
	taskqueue_run(taskqueue_swi_giant);
}

static int
_taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
    cpuset_t *mask, const char *name, va_list ap)
{
	char ktname[MAXCOMLEN + 1];
	struct thread *td;
	struct taskqueue *tq;
	int i, error;

	if (count <= 0)
		return (EINVAL);

	vsnprintf(ktname, sizeof(ktname), name, ap);
	tq = *tqp;

	tq->tq_threads = malloc(sizeof(struct thread *) * count, M_TASKQUEUE,
	    M_NOWAIT | M_ZERO);
	if (tq->tq_threads == NULL) {
		printf("%s: no memory for %s threads\n", __func__, ktname);
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {
		if (count == 1)
			error = kthread_add(taskqueue_thread_loop, tqp, NULL,
			    &tq->tq_threads[i], RFSTOPPED, 0, "%s", ktname);
		else
			error = kthread_add(taskqueue_thread_loop, tqp, NULL,
			    &tq->tq_threads[i], RFSTOPPED, 0,
			    "%s_%d", ktname, i);
		if (error) {
			/* should be ok to continue, taskqueue_free will dtrt */
			printf("%s: kthread_add(%s): error %d", __func__,
			    ktname, error);
			tq->tq_threads[i] = NULL;		/* paranoid */
		} else
			tq->tq_tcount++;
	}
	for (i = 0; i < count; i++) {
		if (tq->tq_threads[i] == NULL)
			continue;
		td = tq->tq_threads[i];
		if (mask) {
			error = cpuset_setthread(td->td_tid, mask);
			/*
			 * Failing to pin is rarely an actual fatal error;
			 * it'll just affect performance.
			 */
			if (error)
				printf("%s: curthread=%llu: can't pin; "
				    "error=%d\n",
				    __func__,
				    (unsigned long long) td->td_tid,
				    error);
		}
		thread_lock(td);
		sched_prio(td, pri);
		sched_add(td, SRQ_BORING);
		thread_unlock(td);
	}

	return (0);
}

int
taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
    const char *name, ...)
{
	va_list ap;
	int error;

	va_start(ap, name);
	error = _taskqueue_start_threads(tqp, count, pri, NULL, name, ap);
	va_end(ap);
	return (error);
}

int
taskqueue_start_threads_cpuset(struct taskqueue **tqp, int count, int pri,
    cpuset_t *mask, const char *name, ...)
{
	va_list ap;
	int error;

	va_start(ap, name);
	error = _taskqueue_start_threads(tqp, count, pri, mask, name, ap);
	va_end(ap);
	return (error);
}

static inline void
taskqueue_run_callback(struct taskqueue *tq,
    enum taskqueue_callback_type cb_type)
{
	taskqueue_callback_fn tq_callback;

	TQ_ASSERT_UNLOCKED(tq);
	tq_callback = tq->tq_callbacks[cb_type];
	if (tq_callback != NULL)
		tq_callback(tq->tq_cb_contexts[cb_type]);
}

void
taskqueue_thread_loop(void *arg)
{
	struct taskqueue **tqp, *tq;

	tqp = arg;
	tq = *tqp;
	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_INIT);
	TQ_LOCK(tq);
	while ((tq->tq_flags & TQ_FLAGS_ACTIVE) != 0) {
		/* XXX ? */
		taskqueue_run_locked(tq);
		/*
		 * Because taskqueue_run() can drop tq_mutex, we need to
		 * check if the TQ_FLAGS_ACTIVE flag wasn't removed in the
		 * meantime, which means we missed a wakeup.
		 */
		if ((tq->tq_flags & TQ_FLAGS_ACTIVE) == 0)
			break;
		TQ_SLEEP(tq, tq, &tq->tq_mutex, 0, "-", 0);
	}
	taskqueue_run_locked(tq);
	/*
	 * This thread is on its way out, so just drop the lock temporarily
	 * in order to call the shutdown callback.  This allows the callback
	 * to look at the taskqueue, even just before it dies.
	 */
	TQ_UNLOCK(tq);
	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_SHUTDOWN);
	TQ_LOCK(tq);

	/* rendezvous with thread that asked us to terminate */
	tq->tq_tcount--;
	wakeup_one(tq->tq_threads);
	TQ_UNLOCK(tq);
	kthread_exit();
}

void
taskqueue_thread_enqueue(void *context)
{
	struct taskqueue **tqp, *tq;

	tqp = context;
	tq = *tqp;
	wakeup_one(tq);
}

TASKQUEUE_DEFINE(swi, taskqueue_swi_enqueue, NULL,
		 swi_add(NULL, "task queue", taskqueue_swi_run, NULL, SWI_TQ,
		     INTR_MPSAFE, &taskqueue_ih));

TASKQUEUE_DEFINE(swi_giant, taskqueue_swi_giant_enqueue, NULL,
		 swi_add(NULL, "Giant taskq", taskqueue_swi_giant_run,
		     NULL, SWI_TQ_GIANT, 0, &taskqueue_giant_ih));

TASKQUEUE_DEFINE_THREAD(thread);

struct taskqueue *
taskqueue_create_fast(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context)
{
	return _taskqueue_create(name, mflags, enqueue, context,
			MTX_SPIN, "fast_taskqueue");
}

static void	*taskqueue_fast_ih;

static void
taskqueue_fast_enqueue(void *context)
{
	swi_sched(taskqueue_fast_ih, 0);
}

static void
taskqueue_fast_run(void *dummy)
{
	taskqueue_run(taskqueue_fast);
}

TASKQUEUE_FAST_DEFINE(fast, taskqueue_fast_enqueue, NULL,
	swi_add(NULL, "fast taskq", taskqueue_fast_run, NULL,
	SWI_TQ_FAST, INTR_MPSAFE, &taskqueue_fast_ih));

int
taskqueue_member(struct taskqueue *queue, struct thread *td)
{
	int i, j, ret = 0;

	for (i = 0, j = 0; ; i++) {
		if (queue->tq_threads[i] == NULL)
			continue;
		if (queue->tq_threads[i] == td) {
			ret = 1;
			break;
		}
		if (++j >= queue->tq_tcount)
			break;
	}
	return (ret);
}

struct taskqgroup_cpu {
	LIST_HEAD(, grouptask)	tgc_tasks;
	struct taskqueue	*tgc_taskq;
	int	tgc_cnt;
	int	tgc_cpu;
};

struct taskqgroup {
	struct taskqgroup_cpu tqg_queue[MAXCPU];
	struct mtx	tqg_lock;
	char *		tqg_name;
	int		tqg_adjusting;
	int		tqg_stride;
	int		tqg_cnt;
};

struct taskq_bind_task {
	struct task bt_task;
	int	bt_cpuid;
};

static void
taskqgroup_cpu_create(struct taskqgroup *qgroup, int idx)
{
	struct taskqgroup_cpu *qcpu;
	int i, j;

	qcpu = &qgroup->tqg_queue[idx];
	LIST_INIT(&qcpu->tgc_tasks);
	qcpu->tgc_taskq = taskqueue_create_fast(NULL, M_WAITOK,
	    taskqueue_thread_enqueue, &qcpu->tgc_taskq);
	taskqueue_start_threads(&qcpu->tgc_taskq, 1, PI_SOFT,
	    "%s_%d", qgroup->tqg_name, idx);

	for (i = CPU_FIRST(), j = 0; j < idx * qgroup->tqg_stride;
	    j++, i = CPU_NEXT(i)) {
		/*
		 * Wait: evaluate the idx * qgroup->tqg_stride'th CPU,
		 * potentially wrapping the actual count
		 */
	}
	qcpu->tgc_cpu = i;
}

static void
taskqgroup_cpu_remove(struct taskqgroup *qgroup, int idx)
{

	taskqueue_free(qgroup->tqg_queue[idx].tgc_taskq);
}

/*
 * Find the taskq with least # of tasks that doesn't currently have any
 * other queues from the uniq identifier.
 */
static int
taskqgroup_find(struct taskqgroup *qgroup, void *uniq)
{
	struct grouptask *n;
	int i, idx, mincnt;
	int strict;

	mtx_assert(&qgroup->tqg_lock, MA_OWNED);
	if (qgroup->tqg_cnt == 0)
		return (0);
	idx = -1;
	mincnt = INT_MAX;
	/*
	 * Two passes;  First scan for a queue with the least tasks that
	 * does not already service this uniq id.  If that fails simply find
	 * the queue with the least total tasks;
	 */
	for (strict = 1; mincnt == INT_MAX; strict = 0) {
		for (i = 0; i < qgroup->tqg_cnt; i++) {
			if (qgroup->tqg_queue[i].tgc_cnt > mincnt)
				continue;
			if (strict) {
				LIST_FOREACH(n,
				    &qgroup->tqg_queue[i].tgc_tasks, gt_list)
					if (n->gt_uniq == uniq)
						break;
				if (n != NULL)
					continue;
			}
			mincnt = qgroup->tqg_queue[i].tgc_cnt;
			idx = i;
		}
	}
	if (idx == -1)
		panic("taskqgroup_find: Failed to pick a qid.");

	return (idx);
}

void
taskqgroup_attach(struct taskqgroup *qgroup, struct grouptask *gtask,
    void *uniq, int irq, char *name)
{
	cpuset_t mask;
	int qid;

	gtask->gt_uniq = uniq;
	gtask->gt_name = name;
	gtask->gt_irq = irq;
	gtask->gt_cpu = -1;
	mtx_lock(&qgroup->tqg_lock);
	qid = taskqgroup_find(qgroup, uniq);
	qgroup->tqg_queue[qid].tgc_cnt++;
	LIST_INSERT_HEAD(&qgroup->tqg_queue[qid].tgc_tasks, gtask, gt_list);
	gtask->gt_taskqueue = qgroup->tqg_queue[qid].tgc_taskq;
	if (irq != -1 && smp_started) {
		CPU_ZERO(&mask);
		CPU_SET(qgroup->tqg_queue[qid].tgc_cpu, &mask);
		mtx_unlock(&qgroup->tqg_lock);
		intr_setaffinity(irq, &mask);
	} else
		mtx_unlock(&qgroup->tqg_lock);
}

int
taskqgroup_attach_cpu(struct taskqgroup *qgroup, struct grouptask *gtask,
	void *uniq, int cpu, int irq, char *name)
{
	cpuset_t mask;
	int i, qid;

	qid = -1;
	gtask->gt_uniq = uniq;
	gtask->gt_name = name;
	gtask->gt_irq = irq;
	gtask->gt_cpu = cpu;
	mtx_lock(&qgroup->tqg_lock);
	if (smp_started) {
		for (i = 0; i < qgroup->tqg_cnt; i++)
			if (qgroup->tqg_queue[i].tgc_cpu == cpu) {
				qid = i;
				break;
			}
		if (qid == -1) {
			mtx_unlock(&qgroup->tqg_lock);
			return (EINVAL);
		}
	} else
		qid = 0;
	qgroup->tqg_queue[qid].tgc_cnt++;
	LIST_INSERT_HEAD(&qgroup->tqg_queue[qid].tgc_tasks, gtask, gt_list);
	gtask->gt_taskqueue = qgroup->tqg_queue[qid].tgc_taskq;
	if (irq != -1 && smp_started) {
		CPU_ZERO(&mask);
		CPU_SET(qgroup->tqg_queue[qid].tgc_cpu, &mask);
		mtx_unlock(&qgroup->tqg_lock);
		intr_setaffinity(irq, &mask);
	} else
		mtx_unlock(&qgroup->tqg_lock);
	return (0);
}

void
taskqgroup_detach(struct taskqgroup *qgroup, struct grouptask *gtask)
{
	int i;

	mtx_lock(&qgroup->tqg_lock);
	for (i = 0; i < qgroup->tqg_cnt; i++)
		if (qgroup->tqg_queue[i].tgc_taskq == gtask->gt_taskqueue)
			break;
	if (i == qgroup->tqg_cnt)
		panic("taskqgroup_detach: task not in group\n");
	qgroup->tqg_queue[i].tgc_cnt--;
	LIST_REMOVE(gtask, gt_list);
	mtx_unlock(&qgroup->tqg_lock);
	gtask->gt_taskqueue = NULL;
}

static void
taskqgroup_binder(void *ctx, int pending)
{
	struct taskq_bind_task *task = (struct taskq_bind_task *)ctx;
	cpuset_t mask;
	int error;

	CPU_ZERO(&mask);
	CPU_SET(task->bt_cpuid, &mask);
	error = cpuset_setthread(curthread->td_tid, &mask);
	thread_lock(curthread);
	sched_bind(curthread, task->bt_cpuid);
	thread_unlock(curthread);

	if (error)
		printf("taskqgroup_binder: setaffinity failed: %d\n",
		    error);
	free(task, M_DEVBUF);
}

static void
taskqgroup_bind(struct taskqgroup *qgroup)
{
	struct taskq_bind_task *task;
	int i;

	/*
	 * Bind taskqueue threads to specific CPUs, if they have been assigned
	 * one.
	 */
	for (i = 0; i < qgroup->tqg_cnt; i++) {
		task = malloc(sizeof (*task), M_DEVBUF, M_NOWAIT);
		TASK_INIT(&task->bt_task, 0, taskqgroup_binder, task);
		task->bt_cpuid = qgroup->tqg_queue[i].tgc_cpu;
		taskqueue_enqueue(qgroup->tqg_queue[i].tgc_taskq,
		    &task->bt_task);
	}
}

static int
_taskqgroup_adjust(struct taskqgroup *qgroup, int cnt, int stride)
{
	LIST_HEAD(, grouptask) gtask_head = LIST_HEAD_INITIALIZER(NULL);
	cpuset_t mask;
	struct grouptask *gtask;
	int i, k, old_cnt, qid, cpu;

	mtx_assert(&qgroup->tqg_lock, MA_OWNED);

	if (cnt < 1 || cnt * stride > mp_ncpus || !smp_started) {
		printf("taskqgroup_adjust failed cnt: %d stride: %d "
		    "mp_ncpus: %d smp_started: %d\n", cnt, stride, mp_ncpus,
		    smp_started);
		return (EINVAL);
	}
	if (qgroup->tqg_adjusting) {
		printf("taskqgroup_adjust failed: adjusting\n");
		return (EBUSY);
	}
	qgroup->tqg_adjusting = 1;
	old_cnt = qgroup->tqg_cnt;
	mtx_unlock(&qgroup->tqg_lock);
	/*
	 * Set up queue for tasks added before boot.
	 */
	if (old_cnt == 0) {
		LIST_SWAP(&gtask_head, &qgroup->tqg_queue[0].tgc_tasks,
		    grouptask, gt_list);
		qgroup->tqg_queue[0].tgc_cnt = 0;
	}

	/*
	 * If new taskq threads have been added.
	 */
	for (i = old_cnt; i < cnt; i++)
		taskqgroup_cpu_create(qgroup, i);
	mtx_lock(&qgroup->tqg_lock);
	qgroup->tqg_cnt = cnt;
	qgroup->tqg_stride = stride;

	/*
	 * Adjust drivers to use new taskqs.
	 */
	for (i = 0; i < old_cnt; i++) {
		while ((gtask = LIST_FIRST(&qgroup->tqg_queue[i].tgc_tasks))) {
			LIST_REMOVE(gtask, gt_list);
			qgroup->tqg_queue[i].tgc_cnt--;
			LIST_INSERT_HEAD(&gtask_head, gtask, gt_list);
		}
	}

	while ((gtask = LIST_FIRST(&gtask_head))) {
		LIST_REMOVE(gtask, gt_list);
		if (gtask->gt_cpu == -1)
			qid = taskqgroup_find(qgroup, gtask->gt_uniq);
		else {
			for (i = 0; i < qgroup->tqg_cnt; i++)
				if (qgroup->tqg_queue[i].tgc_cpu == gtask->gt_cpu) {
					qid = i;
					break;
				}
		}
		qgroup->tqg_queue[qid].tgc_cnt++;
		LIST_INSERT_HEAD(&qgroup->tqg_queue[qid].tgc_tasks, gtask,
		    gt_list);
		gtask->gt_taskqueue = qgroup->tqg_queue[qid].tgc_taskq;
	}
	/*
	 * Set new CPU and IRQ affinity
	 */
	cpu = CPU_FIRST();
	for (i = 0; i < cnt; i++) {
		qgroup->tqg_queue[i].tgc_cpu = cpu;
		for (k = 0; k < qgroup->tqg_stride; k++)
			cpu = CPU_NEXT(cpu);
		CPU_ZERO(&mask);
		CPU_SET(qgroup->tqg_queue[i].tgc_cpu, &mask);
		LIST_FOREACH(gtask, &qgroup->tqg_queue[i].tgc_tasks, gt_list) {
			if (gtask->gt_irq == -1)
				continue;
			intr_setaffinity(gtask->gt_irq, &mask);
		}
	}
	mtx_unlock(&qgroup->tqg_lock);

	/*
	 * If taskq thread count has been reduced.
	 */
	for (i = cnt; i < old_cnt; i++)
		taskqgroup_cpu_remove(qgroup, i);

	mtx_lock(&qgroup->tqg_lock);
	qgroup->tqg_adjusting = 0;

	taskqgroup_bind(qgroup);

	return (0);
}

int
taskqgroup_adjust(struct taskqgroup *qgroup, int cpu, int stride)
{
	int error;

	mtx_lock(&qgroup->tqg_lock);
	error = _taskqgroup_adjust(qgroup, cpu, stride);
	mtx_unlock(&qgroup->tqg_lock);

	return (error);
}

struct taskqgroup *
taskqgroup_create(char *name)
{
	struct taskqgroup *qgroup;

	qgroup = malloc(sizeof(*qgroup), M_TASKQUEUE, M_WAITOK | M_ZERO);
	mtx_init(&qgroup->tqg_lock, "taskqgroup", NULL, MTX_DEF);
	qgroup->tqg_name = name;
	LIST_INIT(&qgroup->tqg_queue[0].tgc_tasks);

	return (qgroup);
}

void
taskqgroup_destroy(struct taskqgroup *qgroup)
{

}
