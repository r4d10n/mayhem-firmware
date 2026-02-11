/*
 * Audio Spectrum Waterfall - External App
 * Live FFT spectrogram display with scrolling waterfall visualization.
 */

#include "ui.hpp"
#include "ui_audio_spectrum.hpp"
#include "ui_navigation.hpp"
#include "external_app.hpp"

namespace ui::external_app::audio_spectrum {
void initialize_app(ui::NavigationView& nav) {
    nav.push<AudioSpectrumView>();
}
}  // namespace ui::external_app::audio_spectrum

extern "C" {

__attribute__((section(".external_app.app_audio_spectrum.application_information"), used)) application_information_t _application_information_audio_spectrum = {
    /*.memory_location = */ (uint8_t*)0x00000000,
    /*.externalAppEntry = */ ui::external_app::audio_spectrum::initialize_app,
    /*.header_version = */ CURRENT_HEADER_VERSION,
    /*.app_version = */ VERSION_MD5,

    /*.app_name = */ "Spectrum",
    /*.bitmap_data = */ {
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
        0xA0, 0x00, 0xA8, 0x02, 0xA8, 0x0A,
        0xAA, 0x0A, 0xAA, 0x2A, 0xAA, 0x2A,
        0xAA, 0x2A, 0xAA, 0x2A, 0xFF, 0xFF,
        0xFF, 0xFF, 0x00, 0x00, 0x55, 0x55,
        0xAA, 0xAA,
    },
    /*.icon_color = */ ui::Color::cyan().v,
    /*.menu_location = */ app_location_t::UTILITIES,
    /*.desired_menu_position = */ -1,

    /*.m4_app_tag = portapack::spi_flash::image_tag_none */ {'P', 'A', 'B', 'P'},
    /*.m4_app_offset = */ 0x00000000,
};
}
