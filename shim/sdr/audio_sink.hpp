#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
// SDL2 intentionally NOT included here — __I/__O macros from hal.h collide
// with GCC x86 intrinsic parameter names in immintrin.h pulled by SDL2.
// SDL_AudioDeviceID is uint32_t; SDL2 included only in audio_sink.cpp.

namespace shim {

struct AudioSample {
    int16_t left;
    int16_t right;
};

class AudioSink {
public:
    static AudioSink& get();  // Singleton like SDL2Backend

    bool init(uint32_t sample_rate = 48000, uint16_t buffer_frames = 1024);
    void shutdown();
    bool is_initialized() const { return device_id_ != 0; }

    // Producer interface (called by baseband thread)
    // Returns pointer to write position in ring buffer, or nullptr if full
    AudioSample* get_write_buffer(size_t count);
    void commit_write(size_t count);

    // Simple write interface
    void write(const AudioSample* samples, size_t count);

    // Beep support
    void beep_start(uint32_t freq_hz, uint32_t sample_rate, uint32_t duration_ms);
    void beep_stop();

    // Volume/rate control
    void set_rate(uint32_t sample_rate);

private:
    AudioSink() = default;

    static void sdl_audio_callback(void* userdata, uint8_t* stream, int len);
    void fill_audio(uint8_t* stream, int len);

    // Lock-free SPSC ring buffer
    static constexpr size_t RING_SIZE = 16384;  // Must be power of 2
    std::vector<AudioSample> ring_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

    uint32_t device_id_ = 0;  // SDL_AudioDeviceID
    uint32_t sample_rate_ = 48000;

    // Beep state
    std::atomic<bool> beep_active_{false};
    uint32_t beep_freq_ = 0;
    uint32_t beep_rate_ = 0;
    uint32_t beep_duration_samples_ = 0;
    uint32_t beep_phase_ = 0;
    uint32_t beep_samples_remaining_ = 0;
};

} // namespace shim
