#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
// SDL2 intentionally NOT included here — __I/__O macros from hal.h collide
// with GCC x86 intrinsic parameter names in immintrin.h pulled by SDL2.
// SDL_AudioDeviceID is uint32_t; SDL2 included only in audio_source.cpp.

namespace shim {

class AudioSource {
public:
    static AudioSource& get();  // Singleton

    bool init(uint32_t sample_rate = 48000, uint16_t buffer_frames = 1024);
    void shutdown();
    bool is_initialized() const { return device_id_ != 0; }

    // Read captured mono samples (returns count actually read)
    size_t read(float* buffer, size_t max_samples);

private:
    AudioSource() = default;

    static void sdl_capture_callback(void* userdata, uint8_t* stream, int len);
    void on_capture(const int16_t* samples, size_t count);

    // SPSC ring buffer for captured audio
    static constexpr size_t RING_SIZE = 16384;  // Must be power of 2
    std::vector<float> ring_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

    uint32_t device_id_ = 0;  // SDL_AudioDeviceID
    uint32_t sample_rate_ = 48000;
};

} // namespace shim
