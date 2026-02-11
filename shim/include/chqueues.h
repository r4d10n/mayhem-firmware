/*
 * ChibiOS Queues Shim for Linux
 * Provides I/O queue types used by USB serial and shell code.
 */

#ifndef _CHQUEUES_H_
#define _CHQUEUES_H_

#include "ch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Input/Output queue callback */
typedef void (*qnotify_t)(void*);

/* These are already defined in ch.h as InputQueue/OutputQueue */

void chIQInit(InputQueue* iqp, uint8_t* bp, size_t size, qnotify_t infy);
void chOQInit(OutputQueue* oqp, uint8_t* bp, size_t size, qnotify_t onfy);
msg_t chIQPutI(InputQueue* iqp, uint8_t b);
size_t chIQReadTimeout(InputQueue* iqp, uint8_t* bp, size_t n, systime_t timeout);
size_t chOQWriteTimeout(OutputQueue* oqp, const uint8_t* bp, size_t n, systime_t timeout);
msg_t chIQGetTimeout(InputQueue* iqp, systime_t timeout);
msg_t chOQPutTimeout(OutputQueue* oqp, uint8_t b, systime_t timeout);

#define chIQGetFullI(iqp) (0)
#define chIQIsEmptyI(iqp) (1)
#define chOQGetFullI(oqp) (0)
#define chOQIsEmptyI(oqp) (1)
#define chOQIsFullI(oqp)  (0)

#ifdef __cplusplus
}
#endif

#endif /* _CHQUEUES_H_ */
