#include <SDL2/SDL.h>   // Must be BEFORE audio_source.hpp to avoid __I/__O collisions
#include "audio_source.hpp"
#include <cstdio>
#include <algorithm>
#include <cstring>

namespace shim {

AudioSource& AudioSource::get() {
    static AudioSource instance;
    return instance;
}

bool AudioSource::init(uint32_t sample_rate, uint16_t buffer_frames) {
    if (device_id_ != 0) {
        fprintf(stderr, "[AudioSource] Already initialized\n");
        return true;
    }

    // Initialize SDL audio subsystem
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[AudioSource] Failed to init SDL audio: %s\n", SDL_GetError());
        return false;
    }

    // Allocate ring buffer
    ring_.resize(RING_SIZE);
    write_pos_.store(0, std::memory_order_relaxed);
    read_pos_.store(0, std::memory_order_relaxed);

    // Configure audio spec for capture (recording)
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;  // Native endian signed 16-bit
    want.channels = 1;            // Mono
    want.samples = buffer_frames;
    want.callback = sdl_capture_callback;
    want.userdata = this;

    // The '1' parameter means capture/recording device
    device_id_ = SDL_OpenAudioDevice(nullptr, 1, &want, &have, 0);
    if (device_id_ == 0) {
        fprintf(stderr, "[AudioSource] Failed to open audio capture device: %s\n", SDL_GetError());
        return false;
    }

    sample_rate_ = have.freq;

    fprintf(stderr, "[AudioSource] Initialized: %d Hz, %d channels, %d samples, format=0x%04x\n",
            have.freq, have.channels, have.samples, have.format);

    // Start capture
    SDL_PauseAudioDevice(device_id_, 0);

    return true;
}

void AudioSource::shutdown() {
    if (device_id_ != 0) {
        SDL_CloseAudioDevice(device_id_);
        device_id_ = 0;
    }
    ring_.clear();
}

void AudioSource::sdl_capture_callback(void* userdata, uint8_t* stream, int len) {
    auto* source = static_cast<AudioSource*>(userdata);
    const auto* samples = reinterpret_cast<const int16_t*>(stream);
    size_t count = len / sizeof(int16_t);
    source->on_capture(samples, count);
}

void AudioSource::on_capture(const int16_t* samples, size_t count) {
    size_t read = read_pos_.load(std::memory_order_acquire);
    size_t write = write_pos_.load(std::memory_order_relaxed);
    size_t available = RING_SIZE - (write - read);

    // Drop samples if buffer is full
    size_t to_write = std::min(count, available);

    for (size_t i = 0; i < to_write; i++) {
        size_t idx = (write + i) & (RING_SIZE - 1);
        // Convert int16 to float [-1.0, 1.0]
        ring_[idx] = samples[i] / 32768.0f;
    }

    write_pos_.store(write + to_write, std::memory_order_release);
}

size_t AudioSource::read(float* buffer, size_t max_samples) {
    size_t read = read_pos_.load(std::memory_order_relaxed);
    size_t write = write_pos_.load(std::memory_order_acquire);
    size_t available = write - read;

    size_t to_read = std::min(max_samples, available);

    for (size_t i = 0; i < to_read; i++) {
        size_t idx = (read + i) & (RING_SIZE - 1);
        buffer[i] = ring_[idx];
    }

    read_pos_.store(read + to_read, std::memory_order_release);

    return to_read;
}

} // namespace shim
