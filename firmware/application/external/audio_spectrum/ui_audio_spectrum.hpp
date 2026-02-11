#ifndef __UI_AUDIO_SPECTRUM_H__
#define __UI_AUDIO_SPECTRUM_H__

#include "ui.hpp"
#include "ui_navigation.hpp"
#include "ui_receiver.hpp"
#include "message.hpp"

#include <array>
#include <cmath>
#include <cstdlib>

using namespace ui;

namespace ui::external_app::audio_spectrum {

class AudioSpectrumView : public View {
   public:
    AudioSpectrumView(NavigationView& nav);
    ~AudioSpectrumView();
    void focus() override;
    void paint(Painter& painter) override;
    std::string title() const override { return "Spectrum"; }

   private:
    NavigationView& nav_;

    static constexpr int screen_width = 240;
    static constexpr int num_bins = 128;
    static constexpr Coord bar_top = 18;
    static constexpr Dim bar_height = 64;
    static constexpr Coord waterfall_top = 98;
    static constexpr Coord waterfall_bottom = 280;

    bool test_mode_{true};
    bool first_paint_{true};
    float test_phase_{0.0f};
    Coord waterfall_y_{waterfall_top};

    std::array<uint8_t, num_bins> current_spectrum_{};

    void generate_test_spectrum();
    void update_display();
    void draw_waterfall_line();

    static Color heat_color(uint8_t v) {
        int val = v;
        if (val < 64)
            return Color(0, 0, val * 4);
        if (val < 128)
            return Color(0, (val - 64) * 4, 255);
        if (val < 192)
            return Color((val - 128) * 4, 255, 255 - (val - 128) * 4);
        return Color(255, 255 - (val - 192) * 4, 0);
    }

    Labels labels{
        {{6 * 8, 0}, "Audio Spectrum", Theme::getInstance()->fg_light->foreground},
        // Frequency scale below bar graph
        {{0, bar_top + bar_height + 2}, "0", Theme::getInstance()->fg_medium->foreground},
        {{7 * 8, bar_top + bar_height + 2}, "6k", Theme::getInstance()->fg_medium->foreground},
        {{14 * 8, bar_top + bar_height + 2}, "12k", Theme::getInstance()->fg_medium->foreground},
        {{22 * 8, bar_top + bar_height + 2}, "18k", Theme::getInstance()->fg_medium->foreground},
        {{28 * 8, bar_top + bar_height + 2}, "24k", Theme::getInstance()->fg_medium->foreground},
    };

    Text text_info{{0, waterfall_bottom + 2, 30 * 8, 16}, "Test pattern"};

    AudioVolumeField field_volume{{21 * 8, waterfall_bottom + 18}};

    Button button_mode{{0, waterfall_bottom + 18, 80, 24}, "Test"};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            if (test_mode_) {
                generate_test_spectrum();
            }
            update_display();
        }};

    MessageHandlerRegistration message_handler_audio_spectrum{
        Message::ID::AudioSpectrum,
        [this](const Message* const p) {
            const auto message = *reinterpret_cast<const AudioSpectrumMessage*>(p);
            for (int i = 0; i < num_bins; i++)
                current_spectrum_[i] = message.data->db[i];
            if (!test_mode_) {
                text_info.set("Live audio");
            }
        }};
};

}  // namespace ui::external_app::audio_spectrum

#endif
