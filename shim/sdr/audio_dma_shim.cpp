/*
 * Audio DMA shim — bridges firmware's audio::dma:: functions to SDL2 AudioSink
 *
 * The baseband processors call audio::dma::tx_empty_buffer() to get a buffer
 * for writing demodulated audio. On real hardware this is a DMA ring buffer.
 * On Linux, we route to the AudioSink's ring buffer which feeds SDL2 audio output.
 */

#include "audio_dma.hpp"
#include "audio_sink.hpp"

#include <cstring>
#include <array>

namespace audio {
namespace dma {

/* Static buffer used by tx_empty_buffer() / rx_empty_buffer().
 * The firmware writes 32 samples per call, matching the DMA buffer size. */
static constexpr size_t BLOCK_SIZE = 32;
static std::array<audio::sample_t, BLOCK_SIZE> tx_buffer;
static std::array<audio::sample_t, BLOCK_SIZE> rx_buffer;

/* Track whether we have a pending write to flush */
static bool tx_buffer_pending = false;

void init_audio_out() {
    /* AudioSink is initialized in main_linux.cpp.
     * This just resets state for a new baseband session. */
    tx_buffer_pending = false;
    std::memset(tx_buffer.data(), 0, sizeof(tx_buffer));
}

void init_audio_in() {
    std::memset(rx_buffer.data(), 0, sizeof(rx_buffer));
}

void disable() {
    /* Flush any pending audio */
    if (tx_buffer_pending) {
        auto& sink = shim::AudioSink::get();
        if (sink.is_initialized()) {
            auto* samples = reinterpret_cast<shim::AudioSample*>(tx_buffer.data());
            sink.write(samples, BLOCK_SIZE);
        }
        tx_buffer_pending = false;
    }
}

void shrink_tx_buffer(bool shrink) {
    (void)shrink;
}

audio::buffer_t tx_empty_buffer() {
    /* If there's a previous buffer pending, flush it to AudioSink */
    if (tx_buffer_pending) {
        auto& sink = shim::AudioSink::get();
        if (sink.is_initialized()) {
            /* audio::sample_t has same layout as shim::AudioSample (left, right int16_t) */
            auto* samples = reinterpret_cast<shim::AudioSample*>(tx_buffer.data());
            sink.write(samples, BLOCK_SIZE);
        }
    }

    /* Clear buffer and return it for the processor to fill */
    std::memset(tx_buffer.data(), 0, sizeof(tx_buffer));
    tx_buffer_pending = true;

    return { tx_buffer.data(), BLOCK_SIZE };
}

audio::buffer_t rx_empty_buffer() {
    /* Read audio input from AudioSink's input ring (fed by WebUI or silence) */
    auto& sink = shim::AudioSink::get();
    auto* samples = reinterpret_cast<shim::AudioSample*>(rx_buffer.data());
    size_t got = sink.read_input(samples, BLOCK_SIZE);
    /* Zero-fill any remaining samples */
    if (got < BLOCK_SIZE) {
        std::memset(&rx_buffer[got], 0, (BLOCK_SIZE - got) * sizeof(audio::sample_t));
    }
    return { rx_buffer.data(), BLOCK_SIZE };
}

void beep_start(uint32_t freq, uint32_t sample_rate, uint32_t beep_duration_ms) {
    auto& sink = shim::AudioSink::get();
    if (sink.is_initialized()) {
        sink.beep_start(freq, sample_rate, beep_duration_ms);
    }
}

void beep_stop() {
    auto& sink = shim::AudioSink::get();
    if (sink.is_initialized()) {
        sink.beep_stop();
    }
}

} /* namespace dma */
} /* namespace audio */
