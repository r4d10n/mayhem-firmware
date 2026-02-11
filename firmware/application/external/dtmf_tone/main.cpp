/*
 * DTMF Tone Generator - External App
 * Generates DTMF (Dual-Tone Multi-Frequency) audio tones via keypad.
 */

#include "ui.hpp"
#include "ui_dtmf_tone.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"

namespace ui::external_app::dtmf_tone {
void initialize_app(ui::NavigationView& nav) {
    nav.push<DTMFToneView>();
}
}  // namespace ui::external_app::dtmf_tone

extern "C" {

__attribute__((section(".external_app.app_dtmf_tone.application_information"), used)) application_information_t _application_information_dtmf_tone = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::dtmf_tone::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,

    /*.app_name = */ "DTMF Tones",
    /*.bitmap_data = */ {
        0x00, 0x00, 0x00, 0x00, 0x98, 0x19,
        0x98, 0x19, 0x00, 0x00, 0x98, 0x19,
        0x98, 0x19, 0x00, 0x00, 0x98, 0x19,
        0x98, 0x19, 0x00, 0x00, 0x98, 0x19,
        0x98, 0x19, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,
    },
    /*.icon_color = */ ui::Color::orange().v,
    /*.menu_location = */ app_location_t::UTILITIES,
    /*.desired_menu_position = */ -1,

    /*.m4_app_tag = portapack::spi_flash::image_tag_none */ {'P', 'A', 'B', 'P'},
    /*.m4_app_offset = */ 0x00000000,
};
}
