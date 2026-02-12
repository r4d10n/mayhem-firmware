#include <SDL2/SDL.h>   // Must be BEFORE audio_sink.hpp to avoid __I/__O collisions
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace shim {

AudioSink& AudioSink::get() {
    static AudioSink instance;
    return instance;
}

bool AudioSink::init(uint32_t sample_rate, uint16_t buffer_frames) {
    if (device_id_ != 0) {
        fprintf(stderr, "[AudioSink] Already initialized\n");
        return true;
    }

    // Initialize SDL audio subsystem
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[AudioSink] Failed to init SDL audio: %s\n", SDL_GetError());
        return false;
    }

    // Allocate ring buffers
    ring_.resize(RING_SIZE);
    write_pos_.store(0, std::memory_order_relaxed);
    read_pos_.store(0, std::memory_order_relaxed);

    input_ring_.resize(RING_SIZE);
    input_write_pos_.store(0, std::memory_order_relaxed);
    input_read_pos_.store(0, std::memory_order_relaxed);

    // Configure audio spec
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;  // Native endian signed 16-bit
    want.channels = 2;            // Stereo
    want.samples = buffer_frames;
    want.callback = sdl_audio_callback;
    want.userdata = this;

    device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_id_ == 0) {
        fprintf(stderr, "[AudioSink] Failed to open audio device: %s\n", SDL_GetError());
        return false;
    }

    sample_rate_ = have.freq;

    fprintf(stderr, "[AudioSink] Initialized: %d Hz, %d channels, %d samples, format=0x%04x\n",
            have.freq, have.channels, have.samples, have.format);

    // Start playback
    SDL_PauseAudioDevice(device_id_, 0);

    return true;
}

void AudioSink::shutdown() {
    if (device_id_ != 0) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    ring_.clear();
}

void AudioSink::sdl_audio_callback(void* userdata, uint8_t* stream, int len) {
    auto* sink = static_cast<AudioSink*>(userdata);
    sink->fill_audio(stream, len);
}

void AudioSink::mix_beep(AudioSample* out, size_t frames) {
    if (!beep_active_.load(std::memory_order_acquire)) return;

    for (size_t i = 0; i < frames && beep_samples_remaining_ > 0; i++) {
        // Generate sine wave at beep_freq_
        float t = static_cast<float>(beep_phase_) / static_cast<float>(beep_rate_);
        int16_t val = static_cast<int16_t>(8000.0f * sinf(2.0f * M_PI * beep_freq_ * t));

        // Mix (add) beep to existing audio with saturation
        int32_t left = static_cast<int32_t>(out[i].left) + val;
        int32_t right = static_cast<int32_t>(out[i].right) + val;
        out[i].left = static_cast<int16_t>(std::clamp(left, -32768, 32767));
        out[i].right = static_cast<int16_t>(std::clamp(right, -32768, 32767));

        beep_phase_++;
        beep_samples_remaining_--;
    }

    if (beep_samples_remaining_ == 0) {
        beep_active_.store(false, std::memory_order_relaxed);
    }
}

void AudioSink::fill_audio(uint8_t* stream, int len) {
    auto* out = reinterpret_cast<AudioSample*>(stream);
    size_t frames_requested = len / sizeof(AudioSample);

    // Read available samples from ring buffer
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = write - read;

    size_t frames_to_copy = std::min(frames_requested, available);

    // Copy samples from ring buffer
    for (size_t i = 0; i < frames_to_copy; i++) {
        size_t idx = (read + i) & (RING_SIZE - 1);
        out[i] = ring_[idx];
    }

    // Update read position
    read_pos_.store(read + frames_to_copy, std::memory_order_release);

    // Fill remainder with silence if underrun
    if (frames_to_copy < frames_requested) {
        std::memset(&out[frames_to_copy], 0, (frames_requested - frames_to_copy) * sizeof(AudioSample));
    }

    // Mix beep tone if active
    mix_beep(out, frames_requested);
}

void AudioSink::set_webui_active(bool active) {
    webui_active_.store(active, std::memory_order_relaxed);
    if (device_id_ != 0) {
        // Pause or resume SDL2 playback device
        SDL_PauseAudioDevice(device_id_, active ? 1 : 0);
    }
    // Also pause/resume SDL2 capture (AudioSource) since mic comes from WebUI
    auto& source = shim::AudioSource::get();
    if (source.is_initialized()) {
        // AudioSource stores device_id_ privately; we use SDL2's pause directly
        // by toggling through AudioSource's init state.
        // For simplicity, we just log - the WebUI mic data goes to input_ring_ instead.
    }
    fprintf(stderr, "[AudioSink] WebUI mode %s\n", active ? "ON (SDL2 paused)" : "OFF (SDL2 resumed)");
}

size_t AudioSink::read_output(AudioSample* buffer, size_t max_count) {
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = write - read;

    size_t to_read = std::min(max_count, available);

    for (size_t i = 0; i < to_read; i++) {
        size_t idx = (read + i) & (RING_SIZE - 1);
        buffer[i] = ring_[idx];
    }

    read_pos_.store(read + to_read, std::memory_order_release);

    // Fill remainder with silence
    if (to_read < max_count) {
        std::memset(&buffer[to_read], 0, (max_count - to_read) * sizeof(AudioSample));
    }

    // Mix beep tone
    mix_beep(buffer, to_read > 0 ? to_read : max_count);

    return to_read;
}

void AudioSink::write_input(const AudioSample* samples, size_t count) {
    size_t read = input_read_pos_.load(std::memory_order_acquire);
    size_t write = input_write_pos_.load(std::memory_order_relaxed);
    size_t available = RING_SIZE - (write - read);

    size_t to_write = std::min(count, available);

    for (size_t i = 0; i < to_write; i++) {
        size_t idx = (write + i) & (RING_SIZE - 1);
        input_ring_[idx] = samples[i];
    }

    input_write_pos_.store(write + to_write, std::memory_order_release);
}

size_t AudioSink::read_input(AudioSample* buffer, size_t max_count) {
    size_t read = input_read_pos_.load(std::memory_order_relaxed);
    size_t write = input_write_pos_.load(std::memory_order_acquire);
    size_t available = write - read;

    size_t to_read = std::min(max_count, available);

    for (size_t i = 0; i < to_read; i++) {
        size_t idx = (read + i) & (RING_SIZE - 1);
        buffer[i] = input_ring_[idx];
    }

    input_read_pos_.store(read + to_read, std::memory_order_release);

    return to_read;
}

AudioSample* AudioSink::get_write_buffer(size_t count) {
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t available = RING_SIZE - (write - read);

    if (available < count) {
        return nullptr;  // Not enough space
    }

    // Check contiguous space before wrap
    size_t write_idx = write & (RING_SIZE - 1);
    size_t contiguous = RING_SIZE - write_idx;

    if (contiguous < count) {
        return nullptr;  // Would wrap — caller should use smaller count or retry
    }

    return &ring_[write_idx];
}

void AudioSink::commit_write(size_t count) {
    size_t write = write_pos_.load(std::memory_order_relaxed);
    write_pos_.store(write + count, std::memory_order_release);
}

void AudioSink::write(const AudioSample* samples, size_t count) {
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t available = RING_SIZE - (write - read);

    // Clamp to available space to avoid overwriting unread data
    size_t to_write = std::min(count, available);

    for (size_t i = 0; i < to_write; i++) {
        size_t idx = (write + i) & (RING_SIZE - 1);
        ring_[idx] = samples[i];
    }

    write_pos_.store(write + to_write, std::memory_order_release);
}

void AudioSink::beep_start(uint32_t freq_hz, uint32_t sample_rate, uint32_t duration_ms) {
    (void)sample_rate;  /* Use actual SDL output rate for correct pitch */
    beep_freq_ = freq_hz;
    beep_rate_ = sample_rate_;  /* SDL output rate (48000), not message rate */
    beep_duration_samples_ = (duration_ms * sample_rate_) / 1000;
    beep_phase_ = 0;
    beep_samples_remaining_ = beep_duration_samples_;
    beep_active_.store(true, std::memory_order_release);
}

void AudioSink::beep_stop() {
    beep_active_.store(false, std::memory_order_release);
}

void AudioSink::set_rate(uint32_t sample_rate) {
    if (sample_rate == sample_rate_) {
        return;  // No change
    }

    // Simple approach: store the rate (SDL2 will handle resampling)
    // For full control, would need to close and reopen device
    sample_rate_ = sample_rate;
}

} // namespace shim
