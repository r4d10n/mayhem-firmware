#ifndef __UI_DTMF_TONE_H__
#define __UI_DTMF_TONE_H__

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "message.hpp"

using namespace ui;

namespace ui::external_app::dtmf_tone {

class DTMFToneView : public View {
   public:
    DTMFToneView(NavigationView& nav);
    ~DTMFToneView();
    void focus() override;
    std::string title() const override { return "DTMF Tones"; }

   private:
    NavigationView& nav_;

    static constexpr uint32_t sample_rate = 48000;
    static constexpr uint32_t tone_duration_ms = 200;
    static constexpr size_t samples_per_chunk = 1024;

    // DTMF frequency tables
    static constexpr uint32_t row_freq[4] = {697, 770, 852, 941};
    static constexpr uint32_t col_freq[4] = {1209, 1336, 1477, 1633};
    static constexpr char key_labels[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}};

    std::string sequence{};

    void on_key_press(uint8_t row, uint8_t col);

#ifdef LINUX_SHIM
    void generate_dual_tone(uint32_t freq1, uint32_t freq2, uint32_t duration_ms);
#endif

    Labels labels{
        {{2 * 8, 1 * 16}, "DTMF Tone Generator", Theme::getInstance()->fg_light->foreground},
        {{0, 2 * 16}, "Seq:", Theme::getInstance()->fg_light->foreground},
        // Column frequency headers
        {{2, 5 * 16}, "1209", Theme::getInstance()->fg_medium->foreground},
        {{56, 5 * 16}, "1336", Theme::getInstance()->fg_medium->foreground},
        {{110, 5 * 16}, "1477", Theme::getInstance()->fg_medium->foreground},
        {{164, 5 * 16}, "1633", Theme::getInstance()->fg_medium->foreground},
        // Row frequency labels
        {{216, 6 * 16 + 10}, "697", Theme::getInstance()->fg_medium->foreground},
        {{216, 8 * 16 + 10}, "770", Theme::getInstance()->fg_medium->foreground},
        {{216, 10 * 16 + 10}, "852", Theme::getInstance()->fg_medium->foreground},
        {{216, 12 * 16 + 10}, "941", Theme::getInstance()->fg_medium->foreground},
    };

    Text text_sequence{{4 * 8, 2 * 16, 25 * 8, 16}, ""};
    Text text_tone_info{{0, 3 * 16 + 8, 30 * 8, 16}, ""};

    // 4x4 DTMF keypad — 4 columns x 4 rows
    // Row 0: 697 Hz
    Button button_1{{4, 6 * 16, 48, 36}, "1"};
    Button button_2{{58, 6 * 16, 48, 36}, "2"};
    Button button_3{{112, 6 * 16, 48, 36}, "3"};
    Button button_A{{166, 6 * 16, 48, 36}, "A"};

    // Row 1: 770 Hz
    Button button_4{{4, 8 * 16, 48, 36}, "4"};
    Button button_5{{58, 8 * 16, 48, 36}, "5"};
    Button button_6{{112, 8 * 16, 48, 36}, "6"};
    Button button_B{{166, 8 * 16, 48, 36}, "B"};

    // Row 2: 852 Hz
    Button button_7{{4, 10 * 16, 48, 36}, "7"};
    Button button_8{{58, 10 * 16, 48, 36}, "8"};
    Button button_9{{112, 10 * 16, 48, 36}, "9"};
    Button button_C{{166, 10 * 16, 48, 36}, "C"};

    // Row 3: 941 Hz
    Button button_star{{4, 12 * 16, 48, 36}, "*"};
    Button button_0{{58, 12 * 16, 48, 36}, "0"};
    Button button_hash{{112, 12 * 16, 48, 36}, "#"};
    Button button_D{{166, 12 * 16, 48, 36}, "D"};

    Button button_clear{{4, 15 * 16, 100, 28}, "Clear"};

    AudioVolumeField field_volume{{21 * 8, 15 * 16}};
};

}  // namespace ui::external_app::dtmf_tone

#endif
