/*
 * chprintf Shim for Linux
 * Replaces ChibiOS chprintf with vsnprintf-based implementation.
 */

#ifndef _CHPRINTF_H_
#define _CHPRINTF_H_

#include <stdarg.h>
#include <stddef.h>

#include "hal.h"

#define CHPRINTF_USE_FLOAT FALSE

#ifdef __cplusplus
extern "C" {
#endif

void chvprintf(BaseSequentialStream *chp, const char *fmt, va_list ap);
int chsnprintf(char *str, size_t size, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

static INLINE void chprintf(BaseSequentialStream *chp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    chvprintf(chp, fmt, ap);
    va_end(ap);
}

#endif /* _CHPRINTF_H_ */
