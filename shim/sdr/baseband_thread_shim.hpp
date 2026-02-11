#pragma once

/*
 * Baseband Thread Shim — replaces M4 core execution on Linux
 *
 * When the M0 application calls m4_init(image_tag), this thread:
 * 1. Creates the appropriate processor via the factory
 * 2. Reads IQ samples from SoapySDR (or generates silence)
 * 3. Feeds samples to processor->execute()
 * 4. Handles messages from M0 via shared_memory
 *
 * The processor's execute() method handles demodulation and routes
 * audio through audio::dma → AudioSink → SDL2 speakers.
 */

#include "spi_image.hpp"
#include "baseband_processor.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace shim {

class BasebandThreadShim {
public:
    static BasebandThreadShim& get();

    /* Start processing with the given image tag.
     * Creates the processor and starts the thread. */
    void start(const portapack::spi_flash::image_tag_t& tag);

    /* Stop the current processor and join the thread. */
    void stop();

    /* Is a processor currently running? */
    bool is_running() const { return running_.load(std::memory_order_relaxed); }

private:
    BasebandThreadShim() = default;

    void thread_func();

    std::unique_ptr<BasebandProcessor> processor_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
};

} // namespace shim
