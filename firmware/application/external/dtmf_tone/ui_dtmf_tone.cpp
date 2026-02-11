#include "ui_dtmf_tone.hpp"
#include "string_format.hpp"

#ifdef LINUX_SHIM
#include "audio_sink.hpp"
#include <cmath>
#endif

#ifndef LINUX_SHIM
#include "baseband_api.hpp"
#include "audio.hpp"
#include "portapack.hpp"
using namespace portapack;
#endif

namespace ui::external_app::dtmf_tone {

DTMFToneView::DTMFToneView(NavigationView& nav)
    : nav_{nav} {
#ifndef LINUX_SHIM
    baseband::run_prepared_image(portapack::memory::map::m4_code.base());
#endif

    add_children({&labels,
                  &text_sequence,
                  &text_tone_info,
                  &button_1, &button_2, &button_3, &button_A,
                  &button_4, &button_5, &button_6, &button_B,
                  &button_7, &button_8, &button_9, &button_C,
                  &button_star, &button_0, &button_hash, &button_D,
                  &button_clear,
                  &field_volume});

    // Wire up all 16 keypad buttons
    button_1.on_select = [this](Button&) { on_key_press(0, 0); };
    button_2.on_select = [this](Button&) { on_key_press(0, 1); };
    button_3.on_select = [this](Button&) { on_key_press(0, 2); };
    button_A.on_select = [this](Button&) { on_key_press(0, 3); };

    button_4.on_select = [this](Button&) { on_key_press(1, 0); };
    button_5.on_select = [this](Button&) { on_key_press(1, 1); };
    button_6.on_select = [this](Button&) { on_key_press(1, 2); };
    button_B.on_select = [this](Button&) { on_key_press(1, 3); };

    button_7.on_select = [this](Button&) { on_key_press(2, 0); };
    button_8.on_select = [this](Button&) { on_key_press(2, 1); };
    button_9.on_select = [this](Button&) { on_key_press(2, 2); };
    button_C.on_select = [this](Button&) { on_key_press(2, 3); };

    button_star.on_select = [this](Button&) { on_key_press(3, 0); };
    button_0.on_select = [this](Button&) { on_key_press(3, 1); };
    button_hash.on_select = [this](Button&) { on_key_press(3, 2); };
    button_D.on_select = [this](Button&) { on_key_press(3, 3); };

    button_clear.on_select = [this](Button&) {
        sequence.clear();
        text_sequence.set("");
        text_tone_info.set("");
#ifndef LINUX_SHIM
        baseband::request_beep_stop();
#endif
    };

    field_volume.set_value(0);
    field_volume.set_value(80);

#ifdef LINUX_SHIM
    // Initialize AudioSink for Linux shim
    shim::AudioSink::get().init(sample_rate, samples_per_chunk);
#else
    audio::set_rate(audio::Rate::Hz_24000);
    audio::output::start();
#endif

    text_tone_info.set("Press a key to play tone");
}

DTMFToneView::~DTMFToneView() {
#ifdef LINUX_SHIM
    shim::AudioSink::get().shutdown();
#else
    baseband::request_beep_stop();
    baseband::shutdown();
    audio::output::stop();
#endif
}

void DTMFToneView::focus() {
    button_5.focus();
}

void DTMFToneView::on_key_press(uint8_t row, uint8_t col) {
    char key = key_labels[row][col];
    sequence += key;
    if (sequence.size() > 25)
        sequence = sequence.substr(sequence.size() - 25);
    text_sequence.set(sequence);

    uint32_t rf = row_freq[row];
    uint32_t cf = col_freq[col];
    text_tone_info.set(to_string_dec_uint(rf) + "+" + to_string_dec_uint(cf) + "Hz [" + std::string(1, key) + "]");

#ifdef LINUX_SHIM
    // Generate dual-tone DTMF signal (proper DTMF)
    generate_dual_tone(rf, cf, tone_duration_ms);
#else
    // Use single-tone baseband beep for ARM (legacy behavior)
    baseband::request_audio_beep(rf, sample_rate, tone_duration_ms);
#endif
}

#ifdef LINUX_SHIM
void DTMFToneView::generate_dual_tone(uint32_t freq1, uint32_t freq2, uint32_t duration_ms) {
    // Calculate total samples needed
    size_t total_samples = (duration_ms * sample_rate) / 1000;

    constexpr float amplitude = 6000.0f;  // Per tone (mixed peak ~12000)
    constexpr float two_pi = 2.0f * M_PI;
    const float phase1_inc = two_pi * static_cast<float>(freq1) / static_cast<float>(sample_rate);
    const float phase2_inc = two_pi * static_cast<float>(freq2) / static_cast<float>(sample_rate);

    float phase1 = 0.0f;
    float phase2 = 0.0f;

    // Generate and write samples in chunks
    constexpr size_t chunk_size = 1024;
    shim::AudioSample buffer[chunk_size];

    size_t samples_remaining = total_samples;
    while (samples_remaining > 0) {
        size_t current_chunk = (samples_remaining < chunk_size) ? samples_remaining : chunk_size;

        for (size_t i = 0; i < current_chunk; i++) {
            // Generate dual-tone DTMF: mix two sine waves
            float sample1 = amplitude * sinf(phase1);
            float sample2 = amplitude * sinf(phase2);
            int16_t mixed = static_cast<int16_t>(sample1 + sample2);

            buffer[i].left = mixed;
            buffer[i].right = mixed;

            // Advance phases
            phase1 += phase1_inc;
            phase2 += phase2_inc;

            // Wrap phases to prevent accumulation errors
            if (phase1 >= two_pi) phase1 -= two_pi;
            if (phase2 >= two_pi) phase2 -= two_pi;
        }

        // Write to AudioSink
        shim::AudioSink::get().write(buffer, current_chunk);
        samples_remaining -= current_chunk;
    }
}
#endif

}  // namespace ui::external_app::dtmf_tone
