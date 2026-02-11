#include "ui_dtmf_tone.hpp"
#include "baseband_api.hpp"
#include "audio.hpp"
#include "portapack.hpp"
#include "string_format.hpp"

using namespace portapack;

namespace ui::external_app::dtmf_tone {

DTMFToneView::DTMFToneView(NavigationView& nav)
    : nav_{nav} {
    baseband::run_prepared_image(portapack::memory::map::m4_code.base());

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
        baseband::request_beep_stop();
    };

    field_volume.set_value(0);
    field_volume.set_value(80);

    audio::set_rate(audio::Rate::Hz_24000);
    audio::output::start();

    text_tone_info.set("Press a key to play tone");
}

DTMFToneView::~DTMFToneView() {
    baseband::request_beep_stop();
    baseband::shutdown();
    audio::output::stop();
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

    // Play the row frequency tone (single-tone; true DTMF requires dual-tone mixing)
    baseband::request_audio_beep(rf, sample_rate, tone_duration_ms);
}

}  // namespace ui::external_app::dtmf_tone
