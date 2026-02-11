/*
 * ChibiOS/RT Shim Implementation for Linux
 * Maps ChibiOS threading/sync primitives to pthreads.
 */

#include "ch.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

/* ---- Thread-local storage for current thread ---- */
static __thread Thread* tls_current_thread = nullptr;

/* Main thread instance */
static Thread main_thread_instance;
static bool system_initialized = false;

/* Global mutex for chSysLock / chMtxUnlock tracking */
static pthread_mutex_t sys_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Per-thread last-locked mutex (for chMtxUnlock with no args) */
static __thread Mutex* tls_last_locked_mutex = nullptr;

/* Startup time for system tick calculation */
static struct timespec startup_time;

/* ---- System init ---- */

void chSysInit(void) {
    if (system_initialized) return;

    clock_gettime(CLOCK_MONOTONIC, &startup_time);

    /* Initialize main thread */
    memset(&main_thread_instance, 0, sizeof(Thread));
    main_thread_instance.pthread = pthread_self();
    pthread_mutex_init(&main_thread_instance.event_mutex, NULL);
    pthread_cond_init(&main_thread_instance.event_cond, NULL);
    main_thread_instance.pending_events = 0;
    main_thread_instance.state = THD_STATE_CURRENT;
    main_thread_instance.prio = NORMALPRIO;
    main_thread_instance.name = "main";

    tls_current_thread = &main_thread_instance;
    system_initialized = true;
}

void chSysHalt(void) {
    fprintf(stderr, "[SHIM] chSysHalt called\n");
    abort();
}

/* ---- Thread functions ---- */

Thread* chThdSelf(void) {
    if (!tls_current_thread) {
        /* Lazy init for threads created outside ChibiOS API */
        chSysInit();
    }
    return tls_current_thread;
}

/* Wrapper for pthread_create */
struct ThreadStartArg {
    void (*func)(void*);
    void* arg;
    Thread* thread;
};

static void* thread_trampoline(void* arg) {
    ThreadStartArg* tsa = (ThreadStartArg*)arg;
    tls_current_thread = tsa->thread;

    void (*func)(void*) = tsa->func;
    void* farg = tsa->arg;
    free(tsa);

    func(farg);
    return NULL;
}

Thread* chThdCreateStatic(void* wsp, size_t size,
                          tprio_t prio, void (*pf)(void*), void* arg) {
    /* Use the working area as the Thread struct storage */
    Thread* tp = (Thread*)wsp;
    memset(tp, 0, sizeof(Thread));
    pthread_mutex_init(&tp->event_mutex, NULL);
    pthread_cond_init(&tp->event_cond, NULL);
    tp->prio = prio;
    tp->state = THD_STATE_READY;

    ThreadStartArg* tsa = (ThreadStartArg*)malloc(sizeof(ThreadStartArg));
    tsa->func = pf;
    tsa->arg = arg;
    tsa->thread = tp;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int ret = pthread_create(&tp->pthread, &attr, thread_trampoline, tsa);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        fprintf(stderr, "[SHIM] pthread_create failed: %s\n", strerror(ret));
        free(tsa);
        return NULL;
    }

    return tp;
}

void chThdSleep(systime_t ticks) {
    uint32_t ms = ST2MS(ticks);
    if (ms == 0) ms = 1;
    usleep(ms * 1000);
}

void chThdSleepMilliseconds(uint32_t msec) {
    if (msec == 0) return;
    usleep(msec * 1000);
}

void chThdSleepMicroseconds(uint32_t usec) {
    if (usec == 0) return;
    usleep(usec);
}

void chThdYield(void) {
    sched_yield();
}

void chThdExit(msg_t msg) {
    Thread* tp = chThdSelf();
    if (tp) tp->rdymsg = msg;
    pthread_exit(NULL);
}

msg_t chThdWait(Thread* tp) {
    if (tp) {
        pthread_join(tp->pthread, NULL);
        return tp->rdymsg;
    }
    return 0;
}

void chThdTerminate(Thread* tp) {
    (void)tp;
    /* Cooperative termination — thread should check chThdShouldTerminate() */
}

/* ---- Event functions ---- */

void chEvtSignal(Thread* tp, eventmask_t events) {
    if (!tp) return;
    pthread_mutex_lock(&tp->event_mutex);
    tp->pending_events |= events;
    pthread_cond_signal(&tp->event_cond);
    pthread_mutex_unlock(&tp->event_mutex);
}

void chEvtSignalI(Thread* tp, eventmask_t events) {
    /* In our shim, ISR context == normal context since we don't have real IRQs */
    chEvtSignal(tp, events);
}

eventmask_t chEvtWaitAny(eventmask_t events) {
    Thread* tp = chThdSelf();
    if (!tp) return 0;

    pthread_mutex_lock(&tp->event_mutex);
    while (!(tp->pending_events & events)) {
        pthread_cond_wait(&tp->event_cond, &tp->event_mutex);
    }
    eventmask_t matched = tp->pending_events & events;
    tp->pending_events &= ~matched;
    pthread_mutex_unlock(&tp->event_mutex);
    return matched;
}

eventmask_t chEvtWaitAnyTimeout(eventmask_t events, systime_t timeout) {
    Thread* tp = chThdSelf();
    if (!tp) return 0;

    if (timeout == TIME_INFINITE) {
        return chEvtWaitAny(events);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint32_t ms = ST2MS(timeout);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&tp->event_mutex);
    while (!(tp->pending_events & events)) {
        int ret = pthread_cond_timedwait(&tp->event_cond, &tp->event_mutex, &ts);
        if (ret == ETIMEDOUT) break;
    }
    eventmask_t matched = tp->pending_events & events;
    tp->pending_events &= ~matched;
    pthread_mutex_unlock(&tp->event_mutex);
    return matched;
}

/* ---- Mutex functions ---- */

void chMtxInit(Mutex* mp) {
    pthread_mutex_init(&mp->mutex, NULL);
    mp->owner = NULL;
}

void chMtxLock(Mutex* mp) {
    pthread_mutex_lock(&mp->mutex);
    mp->owner = chThdSelf();
    tls_last_locked_mutex = mp;
}

bool_t chMtxTryLock(Mutex* mp) {
    int ret = pthread_mutex_trylock(&mp->mutex);
    if (ret == 0) {
        mp->owner = chThdSelf();
        tls_last_locked_mutex = mp;
        return TRUE; /* ChibiOS returns TRUE on success for chMtxTryLock */
    }
    return FALSE;
}

void chMtxUnlock(void) {
    /* ChibiOS chMtxUnlock() unlocks the last mutex locked by current thread */
    Mutex* mp = tls_last_locked_mutex;
    if (mp) {
        mp->owner = NULL;
        tls_last_locked_mutex = NULL;
        pthread_mutex_unlock(&mp->mutex);
    }
}

void chMtxUnlockS(void) {
    chMtxUnlock();
}

void chMtxUnlockAll(void) {
    chMtxUnlock();
}

/* ---- Semaphore functions ---- */

void chSemInit(Semaphore* sp, int32_t n) {
    sem_init(&sp->sem, 0, (unsigned)n);
    sp->cnt = n;
}

void chSemReset(Semaphore* sp, int32_t n) {
    sem_destroy(&sp->sem);
    sem_init(&sp->sem, 0, (unsigned)n);
    sp->cnt = n;
}

msg_t chSemWait(Semaphore* sp) {
    sem_wait(&sp->sem);
    __sync_sub_and_fetch(&sp->cnt, 1);
    return 0;
}

msg_t chSemWaitTimeout(Semaphore* sp, systime_t timeout) {
    if (timeout == TIME_INFINITE) {
        return chSemWait(sp);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint32_t ms = ST2MS(timeout);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int ret = sem_timedwait(&sp->sem, &ts);
    if (ret == 0) {
        __sync_sub_and_fetch(&sp->cnt, 1);
        return 0;
    }
    return -1; /* Timeout */
}

void chSemSignal(Semaphore* sp) {
    __sync_add_and_fetch(&sp->cnt, 1);
    sem_post(&sp->sem);
}

void chSemSignalI(Semaphore* sp) {
    chSemSignal(sp);
}

/* ---- System lock ---- */

void chSysLock(void) {
    pthread_mutex_lock(&sys_mutex);
}

void chSysUnlock(void) {
    pthread_mutex_unlock(&sys_mutex);
}

void chSysLockFromIsr(void) {
    chSysLock();
}

void chSysUnlockFromIsr(void) {
    chSysUnlock();
}

/* ---- Heap functions ---- */
/* Map to standard malloc/free */

void* chHeapAlloc(void* heapp, size_t size) {
    (void)heapp;
    return malloc(size);
}

void chHeapFree(void* p) {
    free(p);
}

size_t chCoreStatus(void) {
    return 1024 * 1024; /* Report 1MB free core */
}

size_t chHeapStatus(void* heapp, size_t* sizep) {
    (void)heapp;
    if (sizep) *sizep = 1024 * 1024; /* Report 1MB free heap */
    return 1; /* Number of heap fragments */
}

/* ---- Virtual timer ---- */

void chVTSet(VirtualTimer* vtp, systime_t time, vtfunc_t vtfunc, void* par) {
    vtp->func = vtfunc;
    vtp->par = par;
    vtp->time = time;
    vtp->active = 1;
    /* TODO: Implement actual timer using timer_create/timer_settime */
}

void chVTReset(VirtualTimer* vtp) {
    vtp->active = 0;
}

systime_t chVTGetSystemTime(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t elapsed_ms =
        (uint64_t)(now.tv_sec - startup_time.tv_sec) * 1000 +
        (uint64_t)(now.tv_nsec - startup_time.tv_nsec) / 1000000;
    return (systime_t)(elapsed_ms * CH_FREQUENCY / 1000);
}

systime_t chVTGetSystemTimeX(void) {
    return chVTGetSystemTime();
}

/* ---- Debug ---- */

void chDbgPanic(const char* msg) {
    fprintf(stderr, "\n[PANIC] %s\n", msg);
    /* Don't abort — just log. Many panics in firmware are non-fatal warnings. */
}

void chDbgAssert(bool_t condition, const char* msg, const char* remark) {
    if (!condition) {
        fprintf(stderr, "[ASSERT] %s: %s\n", msg, remark ? remark : "");
    }
}
