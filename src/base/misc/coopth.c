/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: cooperative threading between dosemu and DOS code.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * This is a V2 implementation of coopthreads in dosemu.
 * V1 was too broken and was removed by commit 158ca93963d968fdc
 */

#include <string.h>
#include <setjmp.h>
#include <assert.h>
#include "emu.h"
#include "utilities.h"
#include "timers.h"
#include "hlt.h"
#include "pcl.h"
#include "coopth.h"

enum CoopthRet { COOPTH_YIELD, COOPTH_WAIT, COOPTH_SLEEP, COOPTH_SCHED,
	COOPTH_DONE, COOPTH_ATTACH, COOPTH_DETACH };
enum CoopthState { COOPTHS_NONE, COOPTHS_STARTING, COOPTHS_RUNNING,
	COOPTHS_SLEEPING, COOPTHS_AWAKEN, COOPTHS_DETACH, COOPTHS_DELETE };
enum CoopthJmp { COOPTH_JMP_NONE, COOPTH_JMP_CANCEL, COOPTH_JMP_EXIT };

struct coopth_thrfunc_t {
    coopth_func_t func;
    void *arg;
};

#define MAX_POST_H 5
#define MAX_UDATA 5

struct coopth_thrdata_t {
    int *tid;
    int attached;
    int leave;
    enum CoopthRet ret;
    void *udata[MAX_UDATA];
    int udata_num;
    struct coopth_thrfunc_t post[MAX_POST_H];
    int posth_num;
    struct coopth_thrfunc_t sleep;
    struct coopth_thrfunc_t clnup;
    jmp_buf exit_jmp;
    int cancelled;
};

struct coopth_ctx_handlers_t {
    coopth_hndl_t pre;
    coopth_hndl_t post;
};

struct coopth_starter_args_t {
    struct coopth_thrfunc_t thr;
    struct coopth_thrdata_t *thrdata;
};

struct coopth_per_thread_t {
    coroutine_t thread;
    enum CoopthState state;
    int set_sleep;
    struct coopth_thrdata_t data;
    struct coopth_starter_args_t args;
    Bit16u ret_cs, ret_ip;
    int dbg;
};

#define MAX_COOP_RECUR_DEPTH 5

struct coopth_t {
    int tid;
    char *name;
    Bit16u hlt_off;
    int off;
    int len;
    int cur_thr;
    int detached;
    int set_sleep;
    struct coopth_ctx_handlers_t ctxh;
    struct coopth_ctx_handlers_t sleeph;
    coopth_hndl_t post;
    struct coopth_per_thread_t pth[MAX_COOP_RECUR_DEPTH];
};

#define MAX_COOPTHREADS 1024
static struct coopth_t coopthreads[MAX_COOPTHREADS];
static int coopth_num;
static int thread_running;
static int joinable_running;
static int threads_joinable;
static int threads_total;
#define MAX_ACT_THRS 10
static int threads_active;
static int active_tids[MAX_ACT_THRS];

static void coopth_callf(struct coopth_t *thr, struct coopth_per_thread_t *pth);
static void coopth_retf(struct coopth_t *thr, struct coopth_per_thread_t *pth);
static void do_del_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth);

#define COOP_STK_SIZE (65536*2)

void coopth_init(void)
{
    co_thread_init();
}

static enum CoopthRet do_run_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    enum CoopthRet ret;
    co_call(pth->thread);
    ret = pth->data.ret;
    switch (ret) {
    case COOPTH_YIELD:
	pth->state = COOPTHS_AWAKEN;
	break;
    case COOPTH_WAIT:
	if (pth->data.attached)
	    dosemu_sleep();
	pth->state = COOPTHS_AWAKEN;
	break;
    case COOPTH_SLEEP:
	pth->state = COOPTHS_SLEEPING;
	break;
    case COOPTH_SCHED:
	break;
    case COOPTH_DETACH:
	pth->state = COOPTHS_DETACH;
	break;
    case COOPTH_DONE:
	if (pth->data.attached)
	    pth->state = COOPTHS_DELETE;
	else
	    do_del_thread(thr, pth);
	break;
    case COOPTH_ATTACH:
	coopth_callf(thr, pth);
	break;
    }
    return ret;
}

static void do_del_thread(struct coopth_t *thr,
	struct coopth_per_thread_t *pth)
{
    int i;
    pth->state = COOPTHS_NONE;
    co_delete(pth->thread);
    thr->cur_thr--;
    if (thr->cur_thr == 0) {
	int found = 0;
	for (i = 0; i < threads_active; i++) {
	    if (active_tids[i] == thr->tid) {
		assert(!found);
		found++;
		continue;
	    }
	    if (found)
		active_tids[i - 1] = active_tids[i];
	}
	assert(found);
	threads_active--;
    }
    threads_total--;

    if (!pth->data.cancelled) {
	for (i = 0; i < pth->data.posth_num; i++)
	    pth->data.post[i].func(pth->data.post[i].arg);
	if (thr->post)
	    thr->post(thr->tid);
    }
}

static void coopth_retf(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    if (!pth->data.attached)
	return;
    threads_joinable--;
    REG(cs) = pth->ret_cs;
    LWORD(eip) = pth->ret_ip;
    if (thr->ctxh.post)
	thr->ctxh.post(thr->tid);
    pth->data.attached = 0;
}

static void coopth_callf(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    if (pth->data.attached)
	return;
    if (thr->ctxh.pre)
	thr->ctxh.pre(thr->tid);
    pth->ret_cs = REG(cs);
    pth->ret_ip = LWORD(eip);
    REG(cs) = BIOS_HLT_BLK_SEG;
    LWORD(eip) = thr->hlt_off;
    threads_joinable++;
    pth->data.attached = 1;
}

static struct coopth_per_thread_t *get_pth(struct coopth_t *thr, int idx)
{
    assert(idx >= 0 && idx < MAX_COOP_RECUR_DEPTH);
    return &thr->pth[idx];
}

static struct coopth_per_thread_t *current_thr(struct coopth_t *thr)
{
    struct coopth_per_thread_t *pth;
    assert(thr - coopthreads < MAX_COOPTHREADS);
    pth = get_pth(thr, thr->cur_thr - 1);
    /* it must be running */
    assert(pth->state > COOPTHS_NONE);
    return pth;
}

static void thread_run(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
again:
    switch (pth->state) {
    case COOPTHS_NONE:
	error("Coopthreads error switch to inactive thread, exiting\n");
	leavedos(2);
	break;
    case COOPTHS_STARTING:
	pth->state = COOPTHS_RUNNING;
	goto again;
	break;
    case COOPTHS_AWAKEN:
	if (thr->sleeph.post)
	    thr->sleeph.post(thr->tid);
	pth->state = COOPTHS_RUNNING;
	/* I hate 'case' without 'break'... so use 'goto' instead. :-)) */
	goto again;
	break;
    case COOPTHS_RUNNING: {
	int jr;
	enum CoopthRet tret;
	if (pth->set_sleep) {
	    pth->set_sleep = 0;
	    pth->state = COOPTHS_SLEEPING;
	    goto again;
	}

	/* We have 2 kinds of recursion:
	 *
	 * 1. (call it recursive thread invocation)
	 *	main_thread -> coopth_start(thread1_func) -> return
	 *		thread1_func() -> coopth_start(thread2_func) -> return
	 *		(thread 1 returned, became zombie)
	 *			thread2_func() -> return
	 *			thread2 joined
	 *		thread1 joined
	 *	main_thread...
	 *
	 * 2. (call it nested thread invocation)
	 *	main_thread -> coopth_start(thread1_func) -> return
	 *		thread1_func() -> do_int_call_back() ->
	 *		run_int_from_hlt() ->
	 *		coopth_start(thread2_func) -> return
	 *			thread2_func() -> return
	 *			thread2 joined
	 *		-> return from do_int_call_back() ->
	 *		return from thread1_func()
	 *		thread1 joined
	 *	main_thread...
	 *
	 * Both cases are supported here, but the nested invocation
	 * is not supposed to be used as being too complex.
	 * Since do_int_call_back() was converted
	 * to coopth API, the nesting is avoided.
	 * If not true, we print an error.
	 */
	jr = joinable_running;
	if (pth->data.attached) {
	    if (joinable_running) {
		static int warned;
		if (!warned) {
		    warned = 1;
		    dosemu_error("Nested thread invocation detected, please fix!\n");
		}
	    }
	    joinable_running++;
	}
	thread_running++;
	tret = do_run_thread(thr, pth);
	thread_running--;
	joinable_running = jr;
	if (tret == COOPTH_SLEEP || tret == COOPTH_WAIT ||
		tret == COOPTH_YIELD) {
	    if (pth->data.sleep.func) {
		/* oneshot sleep handler */
		pth->data.sleep.func(pth->data.sleep.arg);
		pth->data.sleep.func = NULL;
	    }
	    if (thr->sleeph.pre)
		thr->sleeph.pre(thr->tid);
	}
	/* even if the state is still RUNNING, need to break away
	 * as the entry point might change */
	break;
    }
    case COOPTHS_SLEEPING:
	if (pth->data.attached)
	    dosemu_sleep();
	break;
    case COOPTHS_DETACH:
	coopth_retf(thr, pth);
	pth->state = COOPTHS_RUNNING;
	/* cannot goto again here - entry point should change */
	break;
    case COOPTHS_DELETE:
	assert(pth->data.attached);
	coopth_retf(thr, pth);
	do_del_thread(thr, pth);
	break;
    }
}

static void coopth_hlt(Bit32u offs, void *arg)
{
    struct coopth_t *thr = (struct coopth_t *)arg + offs;
    struct coopth_per_thread_t *pth = current_thr(thr);
    thread_run(thr, pth);
}

static void coopth_thread(void *arg)
{
    struct coopth_starter_args_t *args = arg;
    enum CoopthJmp jret;
    if (args->thrdata->cancelled) {
	/* can be cancelled before start - no cleanups set yet */
	args->thrdata->ret = COOPTH_DONE;
	return;
    }
    co_set_data(co_current(), args->thrdata);

    jret = setjmp(args->thrdata->exit_jmp);
    switch (jret) {
    case COOPTH_JMP_NONE:
	args->thr.func(args->thr.arg);
	break;
    case COOPTH_JMP_CANCEL:
	if (args->thrdata->clnup.func)
	    args->thrdata->clnup.func(args->thrdata->clnup.arg);
	break;
    case COOPTH_JMP_EXIT:
	break;
    }

    args->thrdata->ret = COOPTH_DONE;
}

static int register_handler(char *name, void *arg, int len)
{
    emu_hlt_t hlt_hdlr;
    hlt_hdlr.name = name;
    hlt_hdlr.start_addr = -1;
    hlt_hdlr.len = len;
    hlt_hdlr.func = coopth_hlt;
    hlt_hdlr.arg = arg;
    return hlt_register_handler(hlt_hdlr);
}

int coopth_create(char *name)
{
    int num;
    char *nm;
    struct coopth_t *thr;
    if (coopth_num >= MAX_COOPTHREADS) {
	error("Too many threads\n");
	config.exitearly = 1;
	return -1;
    }
    num = coopth_num++;
    nm = strdup(name);
    thr = &coopthreads[num];
    thr->hlt_off = register_handler(nm, thr, 1);
    thr->name = nm;
    thr->cur_thr = 0;
    thr->off = 0;
    thr->tid = num;
    thr->len = 1;

    return num;
}

int coopth_create_multi(char *name, int len)
{
    int i, num;
    char *nm;
    struct coopth_t *thr;
    u_short hlt_off;
    if (coopth_num + len > MAX_COOPTHREADS) {
	error("Too many threads\n");
	config.exitearly = 1;
	return -1;
    }
    num = coopth_num;
    coopth_num += len;
    nm = strdup(name);
    hlt_off = register_handler(nm, &coopthreads[num], len);
    for (i = 0; i < len; i++) {
	thr = &coopthreads[num + i];
	thr->name = nm;
	thr->hlt_off = hlt_off + i;
	thr->cur_thr = 0;
	thr->off = i;
	thr->tid = num + i;
	thr->len = (i == 0 ? len : 1);
    }

    return num;
}

static void check_tid(int tid)
{
    if (tid < 0 || tid >= coopth_num) {
	dosemu_error("Wrong tid\n");
	leavedos(2);
    }
}

int coopth_start(int tid, coopth_func_t func, void *arg)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    int tn;
    check_tid(tid);
    thr = &coopthreads[tid];
    assert(thr->tid == tid);
    if (thr->cur_thr >= MAX_COOP_RECUR_DEPTH) {
	int i;
	error("Coopthreads recursion depth exceeded, %s off=%x\n",
		thr->name, thr->off);
	for (i = 0; i < thr->cur_thr; i++) {
	    error("\tthread %i state %i dbg %#x\n",
		    i, thr->pth[i].state, thr->pth[i].dbg);
	}
	leavedos(2);
    }
    tn = thr->cur_thr++;
    pth = &thr->pth[tn];
    pth->data.tid = &thr->tid;
    pth->data.attached = 0;
    pth->data.posth_num = 0;
    pth->data.sleep.func = NULL;
    pth->data.clnup.func = NULL;
    pth->data.udata_num = 0;
    pth->data.cancelled = 0;
    pth->data.leave = 0;
    pth->args.thr.func = func;
    pth->args.thr.arg = arg;
    pth->args.thrdata = &pth->data;
    pth->set_sleep = thr->set_sleep;
    pth->dbg = LWORD(eax);	// for debug
    pth->thread = co_create(coopth_thread, &pth->args, NULL, COOP_STK_SIZE);
    if (!pth->thread) {
	error("Thread create failure\n");
	leavedos(2);
    }
    pth->state = COOPTHS_STARTING;
    if (tn == 0) {
	assert(threads_active < MAX_ACT_THRS);
	active_tids[threads_active++] = tid;
    }
    threads_total++;
    if (!thr->detached)
	coopth_callf(thr, pth);
    else
	thread_run(thr, pth);
    return 0;
}

int coopth_set_ctx_handlers(int tid, coopth_hndl_t pre, coopth_hndl_t post)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->ctxh.pre = pre;
	thr->ctxh.post = post;
    }
    return 0;
}

int coopth_set_sleep_handlers(int tid, coopth_hndl_t pre, coopth_hndl_t post)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->sleeph.pre = pre;
	thr->sleeph.post = post;
    }
    return 0;
}

int coopth_set_permanent_post_handler(int tid, coopth_hndl_t func)
{
    struct coopth_t *thr;
    int i;
    check_tid(tid);
    for (i = 0; i < coopthreads[tid].len; i++) {
	thr = &coopthreads[tid + i];
	thr->post = func;
    }
    return 0;
}

int coopth_set_detached(int tid)
{
    struct coopth_t *thr;
    check_tid(tid);
    thr = &coopthreads[tid];
    thr->detached = 1;
    return 0;
}

int coopth_init_sleeping(int tid)
{
    struct coopth_t *thr;
    check_tid(tid);
    thr = &coopthreads[tid];
    thr->set_sleep = 1;
    return 0;
}

static int is_main_thr(void)
{
    return (co_get_data(co_current()) == NULL);
}

void coopth_run(void)
{
    int i;
    if (!is_main_thr() || thread_running)
	return;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	struct coopth_per_thread_t *pth = current_thr(thr);
	/* only run detached threads here */
	if (pth->data.attached)
	    continue;
	thread_run(thr, pth);
    }
}

static int __coopth_is_in_thread(int warn, const char *f)
{
    if (!thread_running && warn) {
	static int warned;
	if (!warned) {
	    warned = 1;
	    dosemu_error("Coopth: %s: not in thread!\n", f);
	}
    }
    return thread_running;
}

#define _coopth_is_in_thread() __coopth_is_in_thread(1, __func__)
#define _coopth_is_in_thread_nowarn() __coopth_is_in_thread(0, __func__)

int coopth_get_tid(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    return *thdata->tid;
}

int coopth_set_post_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    assert(thdata->posth_num < MAX_POST_H);
    thdata->post[thdata->posth_num].func = func;
    thdata->post[thdata->posth_num].arg = arg;
    thdata->posth_num++;
    return 0;
}

int coopth_set_sleep_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    thdata->sleep.func = func;
    thdata->sleep.arg = arg;
    return 0;
}

int coopth_set_cleanup_handler(coopth_func_t func, void *arg)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    thdata->clnup.func = func;
    thdata->clnup.arg = arg;
    return 0;
}

void coopth_push_user_data(int tid, void *udata)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.udata_num < MAX_UDATA);
    pth->data.udata[pth->data.udata_num++] = udata;
}

void coopth_push_user_data_cur(void *udata)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    assert(thdata->udata_num < MAX_UDATA);
    thdata->udata[thdata->udata_num++] = udata;
}

void *coopth_pop_user_data(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.udata_num > 0);
    return pth->data.udata[--pth->data.udata_num];
}

void *coopth_pop_user_data_cur(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    assert(thdata->udata_num > 0);
    return thdata->udata[--thdata->udata_num];
}

static void switch_state(enum CoopthRet ret)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    thdata->ret = ret;
    co_resume();
}

static void ensure_attached(void)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    if (!thdata->attached) {
	dosemu_error("Not allowed for detached thread\n");
	leavedos(2);
    }
}

static int is_detached(void)
{
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    assert(thdata);
    return (!thdata->attached);
}

static void check_cancel(void)
{
    /* cancellation point */
    struct coopth_thrdata_t *thdata = co_get_data(co_current());
    if (thdata->cancelled)
	longjmp(thdata->exit_jmp, COOPTH_JMP_CANCEL);
}

static struct coopth_t *on_thread(void)
{
    int i;
    if (REG(cs) != BIOS_HLT_BLK_SEG)
	return NULL;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	if (LWORD(eip) == coopthreads[tid].hlt_off)
	    return &coopthreads[tid];
    }
    return NULL;
}

static int get_scheduled(void)
{
    struct coopth_t *thr = on_thread();
    if (!thr)
	return COOPTH_TID_INVALID;
    return thr->tid;
}

void coopth_yield(void)
{
    assert(_coopth_is_in_thread() && is_detached());
    switch_state(COOPTH_YIELD);
    check_cancel();
}

void coopth_sched(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    /* the check below means that we switch to DOS code, not dosemu code */
    assert(get_scheduled() != coopth_get_tid());
    switch_state(COOPTH_SCHED);
}

void coopth_wait(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    switch_state(COOPTH_WAIT);
    check_cancel();
}

void coopth_sleep(void)
{
    assert(_coopth_is_in_thread());
    switch_state(COOPTH_SLEEP);
    check_cancel();
}

void coopth_attach(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    if (thdata->attached)
	return;
    switch_state(COOPTH_ATTACH);
}

void coopth_exit(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    longjmp(thdata->exit_jmp, COOPTH_JMP_EXIT);
}

void coopth_detach(void)
{
    struct coopth_thrdata_t *thdata;
    assert(_coopth_is_in_thread());
    thdata = co_get_data(co_current());
    if (!thdata->attached)
	return;
    switch_state(COOPTH_DETACH);
}

void coopth_leave(void)
{
    struct coopth_thrdata_t *thdata;
    if (!_coopth_is_in_thread_nowarn())
	return;
    thdata = co_get_data(co_current());
    thdata->leave = 1;
    if (!thdata->attached)
	return;
    switch_state(COOPTH_DETACH);
}

static void do_awake(struct coopth_per_thread_t *pth)
{
    assert(pth->state == COOPTHS_SLEEPING);
    pth->state = COOPTHS_AWAKEN;
}

void coopth_wake_up(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    do_awake(pth);
}

void coopth_asleep(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    /* only support detached threads: I don't see the need for
     * "asynchronously" putting to sleep normal threads. */
    assert(thr->detached);
    pth = current_thr(thr);
    assert(!pth->data.attached);
    /* since we dont know the current state,
     * we cant just change it to SLEEPING here
     * the way we do in coopth_wake_up(). */
    pth->set_sleep = 1;
}

static void do_cancel(struct coopth_t *thr, struct coopth_per_thread_t *pth)
{
    pth->data.cancelled = 1;
    if (pth->data.attached) {
	if (pth->state == COOPTHS_SLEEPING)
	    do_awake(pth);
    } else {
	enum CoopthRet tret = do_run_thread(thr, pth);
	assert(tret == COOPTH_DONE);
    }
}

void coopth_cancel(int tid)
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    if (pth->data.leave)
	return;
    if (_coopth_is_in_thread_nowarn())
	assert(tid != coopth_get_tid());
    do_cancel(thr, pth);
}

static void do_join(struct coopth_per_thread_t *pth, void (*helper)(void))
{
    while (pth->state != COOPTHS_NONE)
	helper();
}

void coopth_join(int tid, void (*helper)(void))
{
    struct coopth_t *thr;
    struct coopth_per_thread_t *pth;
    check_tid(tid);
    thr = &coopthreads[tid];
    pth = current_thr(thr);
    assert(pth->data.attached);
    do_join(pth, helper);
}

/* desperate cleanup attempt, not extremely reliable */
int coopth_flush(void (*helper)(void))
{
    struct coopth_t *thr;
    assert(!_coopth_is_in_thread_nowarn() || is_detached());
    while (threads_joinable) {
	struct coopth_per_thread_t *pth;
	/* the sleeping threads are unlikely to be found here.
	 * This is mainly to flush zombies. */
	thr = on_thread();
	if (!thr)
	    break;
	pth = current_thr(thr);
	assert(pth->data.attached);
	do_cancel(thr, pth);
	do_join(pth, helper);
    }
    if (threads_joinable)
	g_printf("Coopth: %i threads stalled\n", threads_joinable);
    return threads_joinable;
}

void coopth_done(void)
{
    int i, tt, it;
    struct coopth_thrdata_t *thdata = NULL;
    it = _coopth_is_in_thread_nowarn();
    assert(!it || is_detached());
    if (it) {
	thdata = co_get_data(co_current());
	assert(thdata);
    }
    /* there is no safe way to delete joinable threads without joining,
     * so print error only if there are also detached threads left */
    if (threads_total > threads_joinable + it)
	error("Coopth: not all detached threads properly shut down\n");
again:
    tt = threads_total;
    for (i = 0; i < threads_active; i++) {
	int tid = active_tids[i];
	struct coopth_t *thr = &coopthreads[tid];
	struct coopth_per_thread_t *pth = current_thr(thr);
	/* dont cancel own thread */
	if (thdata && *thdata->tid == tid)
	    continue;
	if (!pth->data.attached) {
	    error("\ttid=%i state=%i name=\"%s\" off=%#x\n", tid, pth->state,
		    thr->name, thr->off);
	    do_cancel(thr, pth);
	    assert(threads_total == tt - 1);
	    /* retry the loop as the array changed */
	    goto again;
	} else {
	    g_printf("\ttid=%i state=%i name=%s off=%#x\n", tid, pth->state,
		    thr->name, thr->off);
	}
    }
    /* at this point all detached threads should be killed,
     * except perhaps current one */
    assert(threads_total == threads_joinable + it);
    co_thread_cleanup();
}

int coopth_get_scheduled(void)
{
    assert(_coopth_is_in_thread());
    ensure_attached();
    return get_scheduled();
}
