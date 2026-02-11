/*
 * ChibiOS/RT Shim for Linux
 * Provides ChibiOS types and functions using pthreads.
 * Drop-in replacement for the real ch.h when building for Linux.
 */

#ifndef _CH_H_
#define _CH_H_

#define _CHIBIOS_RT_
#define CH_KERNEL_VERSION "2.6.8-linux-shim"
#define CH_KERNEL_MAJOR 2
#define CH_KERNEL_MINOR 6
#define CH_KERNEL_PATCH 8

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#define CH_SUCCESS FALSE
#define CH_FAILED  TRUE

#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <atomic>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Core types ---- */

typedef uint32_t eventmask_t;
typedef uint32_t systime_t;
typedef uint32_t tprio_t;
typedef int32_t  msg_t;
typedef int      bool_t;
typedef void*    regarm_t;

/* Cortex model constants */
#define CORTEX_M0 0
#define CORTEX_M4 4

/* Simulated tick frequency (1000 Hz = 1ms ticks) */
#define CH_FREQUENCY 1000

/* Event mask helper */
#define EVENT_MASK(n) ((eventmask_t)1 << (n))

/* Thread state enum */
#define THD_STATE_READY   0
#define THD_STATE_CURRENT 1
#define THD_STATE_SLEEP   2

/* Thread structure - wraps pthread */
typedef struct Thread {
    pthread_t           pthread;
    pthread_mutex_t     event_mutex;
    pthread_cond_t      event_cond;
    volatile eventmask_t pending_events;
    volatile int        state;
    tprio_t             prio;
    msg_t               rdymsg;
    const char*         name;
    /* For chThdWait / thread references */
    struct Thread*      newer;
    void*               p_ctx;
} Thread;

/* Working area - just allocate stack on heap for Linux */
#define WORKING_AREA(name, size) \
    uint8_t name[sizeof(Thread) + (size)]

#define THD_WORKING_AREA_SIZE(n) (sizeof(Thread) + (n))
#define THD_WORKING_AREA(name, size) WORKING_AREA(name, size)

/* Mutex */
typedef struct {
    pthread_mutex_t mutex;
    Thread* owner;
} Mutex;

/* Semaphore */
typedef struct {
    sem_t sem;
    int32_t cnt;
} Semaphore;

/* Binary semaphore */
typedef struct {
    Semaphore sem;
} BinarySemaphore;

/* Condition variable */
typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} CondVar;

/* Virtual timer (simplified) */
typedef void (*vtfunc_t)(void*);
typedef struct {
    vtfunc_t func;
    void*    par;
    systime_t time;
    int      active;
} VirtualTimer;

/* Memory pool */
typedef struct {
    void*   next;
    size_t  object_size;
    void*   provider;
} MemoryPool;

/* Exception context (ARM Cortex-M exception stack frame) */
struct extctx {
    regarm_t r0;
    regarm_t r1;
    regarm_t r2;
    regarm_t r3;
    regarm_t r12;
    regarm_t lr_thd;
    regarm_t pc;
    regarm_t xpsr;
};

/* ---- Thread functions ---- */

Thread* chThdSelf(void);

Thread* chThdCreateStatic(void* wsp, size_t size,
                          tprio_t prio, void (*pf)(void*), void* arg);

void chThdSleep(systime_t ticks);
void chThdSleepMilliseconds(uint32_t msec);
void chThdSleepMicroseconds(uint32_t usec);
void chThdYield(void);
void chThdExit(msg_t msg);
msg_t chThdWait(Thread* tp);
void chThdTerminate(Thread* tp);
#define chThdShouldTerminate() 0

/* ---- Event functions ---- */

void chEvtSignal(Thread* tp, eventmask_t events);
void chEvtSignalI(Thread* tp, eventmask_t events);
eventmask_t chEvtWaitAny(eventmask_t events);
eventmask_t chEvtWaitAnyTimeout(eventmask_t events, systime_t timeout);

/* ---- Mutex functions ---- */

void chMtxInit(Mutex* mp);
void chMtxLock(Mutex* mp);
bool_t chMtxTryLock(Mutex* mp);
void chMtxUnlock(void);
void chMtxUnlockS(void);
void chMtxUnlockAll(void);

/* ---- Semaphore functions ---- */

void chSemInit(Semaphore* sp, int32_t n);
void chSemReset(Semaphore* sp, int32_t n);
msg_t chSemWait(Semaphore* sp);
msg_t chSemWaitTimeout(Semaphore* sp, systime_t timeout);
void chSemSignal(Semaphore* sp);
void chSemSignalI(Semaphore* sp);

/* ---- Binary semaphore ---- */

#define chBSemInit(bsp, taken)   chSemInit(&(bsp)->sem, (taken) ? 0 : 1)
#define chBSemWait(bsp)          chSemWait(&(bsp)->sem)
#define chBSemSignal(bsp)        chSemSignal(&(bsp)->sem)
#define chBSemReset(bsp, taken)  chSemReset(&(bsp)->sem, (taken) ? 0 : 1)

/* ---- System lock (global mutex for ISR simulation) ---- */

void chSysLock(void);
void chSysUnlock(void);
void chSysLockFromIsr(void);
void chSysUnlockFromIsr(void);

/* ---- Heap functions ---- */

void* chHeapAlloc(void* heapp, size_t size);
void  chHeapFree(void* p);
size_t chCoreStatus(void);
size_t chHeapStatus(void* heapp, size_t* sizep);

/* ---- Virtual timer ---- */

void  chVTSet(VirtualTimer* vtp, systime_t time, vtfunc_t vtfunc, void* par);
void  chVTReset(VirtualTimer* vtp);
#define chVTIsArmed(vtp) ((vtp)->active)
systime_t chVTGetSystemTime(void);
systime_t chVTGetSystemTimeX(void);

/* ---- Time conversion ---- */

#define MS2ST(msec) ((systime_t)((msec) * CH_FREQUENCY / 1000))
#define US2ST(usec) ((systime_t)((usec) * CH_FREQUENCY / 1000000))
#define ST2MS(ticks) ((uint32_t)((ticks) * 1000 / CH_FREQUENCY))
#define ST2US(ticks) ((uint32_t)((ticks) * 1000000 / CH_FREQUENCY))
#define TIME_INFINITE ((systime_t)-1)
#define TIME_IMMEDIATE ((systime_t)0)

/* ---- Debug ---- */

void chDbgPanic(const char* msg);
void chDbgAssert(bool_t condition, const char* msg, const char* remark);

#define chDbgCheck(c, func) do { if (!(c)) chDbgPanic(#func); } while(0)

/* ---- System init ---- */

void chSysInit(void);
void chSysHalt(void);

/* ---- Misc ---- */

/* Queue types (simplified stubs) */
typedef struct {
    uint8_t* buffer;
    size_t   size;
    size_t   head;
    size_t   tail;
} InputQueue;

typedef struct {
    uint8_t* buffer;
    size_t   size;
    size_t   head;
    size_t   tail;
} OutputQueue;

/* Registry - simplified */
#define chRegFirstThread() chThdSelf()
#define chRegNextThread(tp) ((Thread*)NULL)

/* Scheduler - ISR context */
#define chSchReadyI(tp) ((void)(tp))

/* IRQ handler macros */
#define CH_IRQ_HANDLER(name) void name(void)
#define CH_IRQ_PROLOGUE() do {} while(0)
#define CH_IRQ_EPILOGUE() do {} while(0)

/* All-events mask */
#define ALL_EVENTS ((eventmask_t)-1)

/* Priority constants */
#define IDLEPRIO        1
#define LOWPRIO         2
#define NORMALPRIO      64
#define HIGHPRIO        127
#define ABSPRIO         255

/* Thread function typedef */
typedef msg_t (*tfunc_t)(void*);

/* Idle thread working area (unused on Linux) */
#define PORT_IDLE_THREAD_STACK_SIZE 64

/* Dynamic thread creation from heap (stub) */
static inline Thread* chThdCreateFromHeap(void* heapp, size_t size,
    tprio_t prio, tfunc_t pf, void* arg) {
    (void)heapp; (void)size; (void)prio; (void)pf; (void)arg;
    return NULL;
}

/* ---- Performance counter support ---- */
static inline systime_t chTimeNow(void) { return chVTGetSystemTime(); }
static inline Thread* chSysGetIdleThread(void) { return chThdSelf(); }
static inline systime_t chThdGetTicks(Thread* tp) { (void)tp; return 0; }

/* ---- Asynchronous channel (for USB serial driver) ---- */
#define _base_asynchronous_channel_methods                                  \
    int (*put)(void *instance, uint8_t b, systime_t timeout);               \
    int (*get)(void *instance, systime_t timeout);                          \
    size_t (*writet)(void *instance, const uint8_t *bp, size_t n, systime_t timeout); \
    size_t (*readt)(void *instance, uint8_t *bp, size_t n, systime_t timeout);

#ifdef __cplusplus
}
#endif

/* C++ convenience: provide inline overloads */
#ifdef __cplusplus

/* Allow chMtxUnlock to be called in MessageQueue context.
 * ChibiOS chMtxUnlock() has no parameters - it unlocks the last
 * mutex locked by the current thread. Our shim tracks this via TLS. */

#endif /* __cplusplus */

#endif /* _CH_H_ */
