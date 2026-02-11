/*
 * Stub implementations for M4 hardware classes
 *
 * These classes (BasebandThread, RSSIThread, EventDispatcher) are used as
 * member variables by the proc_*.cpp processors. On real hardware they manage
 * SGPIO/DMA/RSSI hardware threads. On Linux they're no-ops — the shim's
 * own baseband thread handles processor execution.
 *
 * Also includes stubs for rf::rssi::dma and w25q80bv SPI flash.
 */

#include "baseband_thread.hpp"
#include "rssi_thread.hpp"

#include <cstdio>

// ============================================================================
// BasebandThread stubs
// ============================================================================

Thread* BasebandThread::thread = nullptr;

BasebandThread::BasebandThread(
    uint32_t sampling_rate,
    BasebandProcessor* const baseband_processor,
    baseband::Direction direction,
    bool auto_start,
    tprio_t priority)
    : baseband_processor_(baseband_processor),
      direction_(direction),
      sampling_rate_(sampling_rate),
      priority_(priority) {
    (void)auto_start;
    // On Linux, processor execution is handled by the shim's baseband thread
}

BasebandThread::~BasebandThread() {
}

void BasebandThread::start() {
    // No-op on Linux — shim manages the baseband processing loop
}

void BasebandThread::set_sampling_rate(uint32_t new_sampling_rate) {
    sampling_rate_ = new_sampling_rate;
}

void BasebandThread::run() {
    // Never called on Linux
}

// ============================================================================
// RSSIThread stubs
// ============================================================================

Thread* RSSIThread::thread = nullptr;

RSSIThread::RSSIThread(bool auto_start, tprio_t priority)
    : priority_(priority) {
    (void)auto_start;
}

RSSIThread::~RSSIThread() {
}

void RSSIThread::start() {
}

void RSSIThread::run() {
}

// The M0 EventDispatcher (event_m0.cpp) drives the UI. But the M4
// EventDispatcher has a different constructor signature that the
// proc_*.cpp renamed-main functions reference. Stub just that constructor.
#include "event_m4.hpp"

EventDispatcher::EventDispatcher(std::unique_ptr<BasebandProcessor> bp)
    : baseband_processor(std::move(bp)) {
}

// ============================================================================
// rf::rssi::dma stubs
// ============================================================================

namespace rf {
namespace rssi {
namespace dma {

void init() {}
void enable() {}
void disable() {}

} /* namespace dma */
} /* namespace rssi */
} /* namespace rf */

// ============================================================================
// w25q80bv SPI flash stubs (for proc_flash_utility)
// ============================================================================

namespace w25q80bv {

void setup() {}
void initialite_spi() {}
void disable_spifi() {}
void remove_write_protection() {}
void erase_chip() {}
void wait_for_device() {}
void wait_not_busy() {}
void write(unsigned long, unsigned char*, unsigned long) {}

} /* namespace w25q80bv */
