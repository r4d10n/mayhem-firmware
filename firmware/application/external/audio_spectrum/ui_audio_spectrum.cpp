#include "ui_audio_spectrum.hpp"
#include "baseband_api.hpp"
#include "audio.hpp"
#include "portapack.hpp"
#include "string_format.hpp"

#ifdef LINUX_SHIM
#include "audio_source.hpp"
#endif

using namespace portapack;

namespace ui::external_app::audio_spectrum {

AudioSpectrumView::AudioSpectrumView(NavigationView& nav)
    : nav_{nav} {
    baseband::run_prepared_image(portapack::memory::map::m4_code.base());

    current_spectrum_.fill(0);

    add_children({&labels,
                  &text_info,
                  &field_volume,
                  &button_mode});

    button_mode.on_select = [this](Button&) {
        test_mode_ = !test_mode_;
        button_mode.set_text(test_mode_ ? "Test" : "Live");
        text_info.set(test_mode_ ? "Test pattern" : "Waiting for audio...");
        if (test_mode_) {
            test_phase_ = 0.0f;
        }
#ifdef LINUX_SHIM
        else {
            // Initialize audio source when entering live mode
            if (!shim::AudioSource::get().is_initialized()) {
                shim::AudioSource::get().init(48000, 1024);
            }
        }
#endif
    };

    field_volume.set_value(0);
    field_volume.set_value(80);

    audio::set_rate(audio::Rate::Hz_48000);
    audio::output::start();
}

AudioSpectrumView::~AudioSpectrumView() {
    baseband::shutdown();
    audio::output::stop();
#ifdef LINUX_SHIM
    shim::AudioSource::get().shutdown();
#endif
}

void AudioSpectrumView::focus() {
    button_mode.focus();
}

void AudioSpectrumView::generate_test_spectrum() {
    test_phase_ += 0.04f;

    float peak_pos = 64.0f + 40.0f * sinf(test_phase_);
    float peak2_pos = 64.0f + 30.0f * sinf(test_phase_ * 2.3f);

    for (int i = 0; i < num_bins; i++) {
        // 1/f noise floor
        float base = 30.0f - i * 0.2f;
        if (base < 5.0f) base = 5.0f;

        // Sweeping primary peak
        float dist = fabsf(i - peak_pos);
        float peak = 200.0f - dist * 5.0f;
        if (peak < 0.0f) peak = 0.0f;

        // Second harmonic
        float dist2 = fabsf(i - peak2_pos);
        float peak2 = 120.0f - dist2 * 4.0f;
        if (peak2 < 0.0f) peak2 = 0.0f;

        // Random noise
        float noise = (float)(rand() % 15);

        float val = base + peak + peak2 + noise;
        if (val > 255.0f) val = 255.0f;
        current_spectrum_[i] = (uint8_t)val;
    }
}

void AudioSpectrumView::update_display() {
    draw_waterfall_line();
    set_dirty();
}

void AudioSpectrumView::draw_waterfall_line() {
    // Draw one line of waterfall directly to display
    for (int i = 0; i < num_bins; i++) {
        Coord x = (Coord)(i * screen_width / num_bins);
        Dim w = (Dim)((i + 1) * screen_width / num_bins - x);
        if (w < 1) w = 1;
        Color c = heat_color(current_spectrum_[i]);
        display.fill_rectangle({x, waterfall_y_, w, 1}, c);
    }

    // Draw thin position marker
    Coord next_y = waterfall_y_ + 1;
    if (next_y >= waterfall_bottom)
        next_y = waterfall_top;
    display.fill_rectangle({0, next_y, (Dim)screen_width, 1}, Color::white());

    waterfall_y_++;
    if (waterfall_y_ >= waterfall_bottom)
        waterfall_y_ = waterfall_top;
}

void AudioSpectrumView::paint(Painter& painter) {
    if (first_paint_) {
        // Clear waterfall area on first draw
        painter.fill_rectangle({0, waterfall_top, (Dim)screen_width, (Dim)(waterfall_bottom - waterfall_top)}, Color::black());
        first_paint_ = false;
    }

    // Bar graph
    for (int i = 0; i < num_bins; i++) {
        Coord x = (Coord)(i * screen_width / num_bins);
        Dim w = (Dim)((i + 1) * screen_width / num_bins - x);
        if (w < 1) w = 1;

        Dim h = (Dim)(current_spectrum_[i] * bar_height / 256);
        if (h > bar_height) h = bar_height;

        // Dark background above bar
        if (bar_height > h)
            painter.fill_rectangle({x, bar_top, w, (Dim)(bar_height - h)}, Color::black());
        // Colored bar
        if (h > 0)
            painter.fill_rectangle({x, (Coord)(bar_top + bar_height - h), w, h}, heat_color(current_spectrum_[i]));
    }
}

#ifdef LINUX_SHIM

void AudioSpectrumView::apply_hanning_window(std::complex<float>* data, size_t n) {
    const float pi = 3.14159265358979323846f;
    for (size_t i = 0; i < n; i++) {
        float window = 0.5f * (1.0f - cosf(2.0f * pi * i / (n - 1)));
        data[i] *= window;
    }
}

void AudioSpectrumView::fft_radix2(std::complex<float>* x, int N) {
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; i++) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }

    // Butterfly stages
    const float pi = 3.14159265358979323846f;
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * pi / len;
        std::complex<float> wlen(cosf(ang), sinf(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                auto u = x[i + j];
                auto v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void AudioSpectrumView::process_live_audio() {
    constexpr size_t FFT_SIZE = 256;
    static std::array<std::complex<float>, FFT_SIZE> fft_buffer;
    static std::array<float, FFT_SIZE> audio_samples;

    // Read audio samples from microphone
    size_t samples_read = shim::AudioSource::get().read(audio_samples.data(), FFT_SIZE);

    if (samples_read < FFT_SIZE) {
        // Not enough samples yet - keep previous spectrum
        return;
    }

    // Convert to complex and apply Hanning window
    for (size_t i = 0; i < FFT_SIZE; i++) {
        fft_buffer[i] = std::complex<float>(audio_samples[i], 0.0f);
    }
    apply_hanning_window(fft_buffer.data(), FFT_SIZE);

    // Compute FFT
    fft_radix2(fft_buffer.data(), FFT_SIZE);

    // Compute magnitude spectrum for positive frequencies (bins 0 to FFT_SIZE/2)
    // Map to num_bins (128) output bins
    for (int i = 0; i < num_bins; i++) {
        // Map output bin i to FFT bin
        int fft_bin = (i * (FFT_SIZE / 2)) / num_bins;

        // Compute magnitude
        float real = fft_buffer[fft_bin].real();
        float imag = fft_buffer[fft_bin].imag();
        float mag = sqrtf(real * real + imag * imag);

        // Convert to dB scale (reference: full scale = 1.0)
        float db = 0.0f;
        if (mag > 1e-6f) {
            db = 20.0f * log10f(mag);
        } else {
            db = -120.0f;
        }

        // Map dB range [-80, 0] to [0, 255]
        // Add gain to make it more visible
        db += 60.0f;  // Shift range to [-20, 60]

        float normalized = (db + 20.0f) / 80.0f;  // Map [-20, 60] to [0, 1]
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        current_spectrum_[i] = (uint8_t)(normalized * 255.0f);
    }

    text_info.set("Live microphone");
}

#endif  // LINUX_SHIM

}  // namespace ui::external_app::audio_spectrum
