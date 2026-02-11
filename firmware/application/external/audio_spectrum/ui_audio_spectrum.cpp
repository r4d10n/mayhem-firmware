#include "ui_audio_spectrum.hpp"
#include "baseband_api.hpp"
#include "audio.hpp"
#include "portapack.hpp"
#include "string_format.hpp"

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
    };

    field_volume.set_value(0);
    field_volume.set_value(80);

    audio::set_rate(audio::Rate::Hz_48000);
    audio::output::start();
}

AudioSpectrumView::~AudioSpectrumView() {
    baseband::shutdown();
    audio::output::stop();
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

}  // namespace ui::external_app::audio_spectrum
