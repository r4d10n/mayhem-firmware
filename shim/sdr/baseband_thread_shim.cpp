/*
 * Baseband Thread Shim — feeds IQ samples to processors on Linux
 *
 * Replaces the M4 core's SGPIO/DMA/EventDispatcher pipeline with a
 * simple loop: read IQ → execute processor → sleep to match sample rate.
 *
 * Without an SDR device, the thread generates silence buffers so
 * processors that only handle messages (like AudioBeep) still work.
 */

#include "baseband_thread_shim.hpp"
#include "processor_factory.hpp"
#include "soapy_sdr_shim.hpp"
#include "portapack_shared_memory.hpp"
#include "dsp_types.hpp"
#include "message.hpp"

#include <cstdio>
#include <cstring>
#include <chrono>
#include <complex>
#include <array>

namespace shim {

BasebandThreadShim& BasebandThreadShim::get() {
    static BasebandThreadShim instance;
    return instance;
}

void BasebandThreadShim::start(const portapack::spi_flash::image_tag_t& tag) {
    /* Stop any existing processor first */
    if (running_.load()) {
        stop();
    }

    /* Create processor from factory */
    processor_ = create_processor(tag);
    if (!processor_) {
        char t[5] = {};
        std::memcpy(t, &tag, 4);
        fprintf(stderr, "[Baseband] No processor for tag '%s'\n", t);
        return;
    }

    fprintf(stderr, "[Baseband] Starting processor: %s\n", processor_name(tag));

    /* Clear any stale message pointer before starting */
    shared_memory.baseband_message = nullptr;

    stop_requested_.store(false);
    running_.store(true);

    /* Signal M0 that baseband is ready */
    shared_memory.set_baseband_ready();

    /* Start the processing thread */
    thread_ = std::thread(&BasebandThreadShim::thread_func, this);
}

void BasebandThreadShim::start_pending() {
    if (!has_pending_tag_) {
        fprintf(stderr, "[Baseband] start_pending() called with no pending tag\n");
        return;
    }
    has_pending_tag_ = false;
    start(pending_tag_);
}

void BasebandThreadShim::stop() {
    if (!running_.load()) return;

    stop_requested_.store(true);

    if (thread_.joinable()) {
        thread_.join();
    }

    processor_.reset();
    running_.store(false);

    fprintf(stderr, "[Baseband] Processor stopped\n");
}

void BasebandThreadShim::dispatch_message() {
    /* Mirror what event_m4.cpp EventDispatcher::handle_baseband_queue() does:
     * Read the message pointer, dispatch to processor, then clear it so
     * send_message() on the M0 side stops spinning. */
    const auto* message = shared_memory.baseband_message;
    if (!message) return;

    if (message->id == Message::ID::Shutdown) {
        stop_requested_.store(true, std::memory_order_relaxed);
    } else if (processor_) {
        processor_->on_message(message);
    }

    /* Clear the pointer — this unblocks send_message() on the M0 thread */
    shared_memory.baseband_message = nullptr;
}

void BasebandThreadShim::thread_func() {
    /* Buffer for IQ samples — 1024 complex8 samples per block,
     * matching the firmware's typical DMA buffer size.
     * iq_sample_t has same layout as complex8_t {int8_t i, q}. */
    static constexpr size_t BLOCK_SIZE = 1024;
    iq_sample_t iq_buffer[BLOCK_SIZE];

    auto& sdr = SoapySDRShim::get();
    const bool have_sdr = sdr.is_open();

    /* Timing: pace the loop to approximate real-time sample rate.
     * Default to 3.072 MHz (common baseband rate).
     * block_duration_us = BLOCK_SIZE / (samples_per_sec / 1e6) */
    const long block_duration_us = (BLOCK_SIZE * 1000000L) / 3072000L;

    while (!stop_requested_.load(std::memory_order_relaxed)) {
        auto loop_start = std::chrono::steady_clock::now();

        /* Check for messages from M0 (beep requests, config, shutdown, etc.)
         * This must happen BEFORE execute() so configuration messages
         * are applied before the next sample block is processed. */
        dispatch_message();

        /* Read IQ samples from SDR or generate silence */
        if (have_sdr) {
            int n = sdr.read_samples(iq_buffer, BLOCK_SIZE, 100000);
            if (n <= 0) {
                /* Read failed or timeout — fill with silence */
                std::memset(iq_buffer, 0, sizeof(iq_buffer));
            }
        } else {
            /* No SDR — generate silence */
            std::memset(iq_buffer, 0, sizeof(iq_buffer));
        }

        /* Feed samples to processor.
         * iq_sample_t and complex8_t have identical layout {int8_t, int8_t}. */
        const buffer_c8_t buffer{
            reinterpret_cast<complex8_t*>(iq_buffer),
            BLOCK_SIZE
        };
        processor_->execute(buffer);

        /* Check messages again after execute() — handles the case where
         * the M0 thread posted a message while we were processing */
        dispatch_message();

        /* Pace the loop to avoid spinning when no SDR is attached */
        if (!have_sdr) {
            auto elapsed = std::chrono::steady_clock::now() - loop_start;
            auto target = std::chrono::microseconds(block_duration_us);
            if (elapsed < target) {
                std::this_thread::sleep_for(target - elapsed);
            }
        }
    }
}

} // namespace shim
