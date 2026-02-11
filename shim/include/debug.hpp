/*
 * Unified debug.hpp shim for Linux
 *
 * Both firmware/application/debug.hpp and firmware/baseband/debug.hpp
 * use the same include guard __DEBUG_H__. Since both directories are
 * in the include path, whichever is found first wins — breaking the other.
 * This shim provides the superset of both, found first via -I ordering.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <cstdint>
#include <string>

// --- Common to both application and baseband debug.hpp ---

extern uint32_t __process_stack_base__;
extern uint32_t __process_stack_end__;
#define CRT0_STACKS_FILL_PATTERN 0x55555555

inline uint32_t get_free_stack_space() {
    uint32_t* p;
    for (p = &__process_stack_base__; *p == CRT0_STACKS_FILL_PATTERN && p < &__process_stack_end__; p++)
        ;
    auto stack_space_left = p - &__process_stack_base__;
    return stack_space_left;
}

// --- From application/debug.hpp ---

void __debug_log(const std::string& msg);
#define __LOG2(l, msg) __debug_log(std::string{#l} + ":" + msg)
#define __LOG1(l, msg) __LOG2(l, msg)
#define DEBUG_LOG(msg) __LOG1(__LINE__, msg)

struct extctx;  // Forward declare — not available on Linux
extern void draw_guru_meditation(uint8_t, const char*);
extern void draw_guru_meditation(uint8_t, const char*, struct extctx*, uint32_t);

bool stack_dump();
bool memory_dump(uint32_t* addr_start, uint32_t num_words, bool stack_flag);

// --- From baseband/debug.hpp (ARM-only macros stubbed out) ---

#ifdef LINUX_SHIM
#define HALT_IF_DEBUGGING()    do { } while(0)
#define HALT_UNTIL_DEBUGGING() do { } while(0)
#else
#define HALT_IF_DEBUGGING()                                 \
    do {                                                    \
        if ((*(volatile uint32_t*)0xE000EDF0) & (1 << 0)) { \
            __asm__ __volatile__("bkpt 1");                 \
        }                                                   \
    } while (0)

#define HALT_UNTIL_DEBUGGING()                                \
    while (!((*(volatile uint32_t*)0xE000EDF0) & (1 << 0))) { \
    }                                                         \
    __asm__ __volatile__("bkpt 1")
#endif

#endif /*__DEBUG_H__*/
