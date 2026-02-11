/*
 * External App Registry — collects all compiled-in external app structs
 *
 * Each external app main.cpp defines a global application_information_t.
 * We declare extern references to all of them and return a vector of
 * pointers for the menu loader to enumerate.
 */

#include "external_app_registry.hpp"

/* Extern declarations for all compiled-in external app info structs.
 * These are defined in each app's main.cpp as extern "C" globals. */
extern "C" {
    extern application_information_t _application_information_afsk_rx;
    extern application_information_t _application_information_calculator;
    extern application_information_t _application_information_font_viewer;
    extern application_information_t _application_information_blespam;
    extern application_information_t _application_information_analogtv;
    extern application_information_t _application_information_nrf_rx;
    extern application_information_t _application_information_coasterp;
    extern application_information_t _application_information_lge;
    extern application_information_t _application_information_lcr;
    extern application_information_t _application_information_jammer;
    extern application_information_t _application_information_gpssim;
    extern application_information_t _application_information_spainter;
    extern application_information_t _application_information_keyfob;
    extern application_information_t _application_information_tetris;
    extern application_information_t _application_information_extsensors;
    extern application_information_t _application_information_foxhunt_rx;
    extern application_information_t _application_information_audio_test;
    extern application_information_t _application_information_wardrivemap;
    extern application_information_t _application_information_tpmsrx;
    extern application_information_t _application_information_protoview;
    extern application_information_t _application_information_adsbtx;
    extern application_information_t _application_information_sstvtx;
    extern application_information_t _application_information_sstvrx;
    extern application_information_t _application_information_random_password;
    extern application_information_t _application_information_wefax_rx;
    extern application_information_t _application_information_noaaapt_rx;
    extern application_information_t _application_information_shoppingcart_lock;
    extern application_information_t _application_information_ookbrute;
    extern application_information_t _application_information_ook_editor;
    extern application_information_t _application_information_cvs_spam;
    extern application_information_t _application_information_flippertx;
    extern application_information_t _application_information_remote;
    extern application_information_t _application_information_mcu_temperature;
    extern application_information_t _application_information_fmradio;
    extern application_information_t _application_information_tuner;
    extern application_information_t _application_information_metronome;
    extern application_information_t _application_information_app_manager;
    extern application_information_t _application_information_hopper;
    extern application_information_t _application_information_antenna_length;
    extern application_information_t _application_information_view_wav;
    extern application_information_t _application_information_sd_wipe;
    extern application_information_t _application_information_playlist_editor;
    extern application_information_t _application_information_snake;
    extern application_information_t _application_information_stopwatch;
    extern application_information_t _application_information_breakout;
    extern application_information_t _application_information_dinogame;
    extern application_information_t _application_information_doom;
    extern application_information_t _application_information_debug_pmem;
    extern application_information_t _application_information_scanner;
    extern application_information_t _application_information_level;
    extern application_information_t _application_information_gfxeq;
    extern application_information_t _application_information_waterfall_designer;
    extern application_information_t _application_information_detector_rx;
    extern application_information_t _application_information_spaceinv;
    extern application_information_t _application_information_blackjack;
    extern application_information_t _application_information_battleship;
    extern application_information_t _application_information_ert;
    extern application_information_t _application_information_epirb_rx;
    extern application_information_t _application_information_soundboard;
    extern application_information_t _application_information_game2048;
    extern application_information_t _application_information_bht_tx;
    extern application_information_t _application_information_morse_practice;
    extern application_information_t _application_information_adult_toys_controller;
    extern application_information_t _application_information_flex_rx;
    extern application_information_t _application_information_subcarrx;
    extern application_information_t _application_information_siggen;
    extern application_information_t _application_information_sdusb;
    extern application_information_t _application_information_morse_radio;
    extern application_information_t _application_information_morseradiotx;
    extern application_information_t _application_information_dtmf_tone;
    extern application_information_t _application_information_audio_spectrum;
}

namespace shim {

const std::vector<application_information_t*>& get_external_apps() {
    static const std::vector<application_information_t*> apps = {
        &_application_information_afsk_rx,
        &_application_information_calculator,
        &_application_information_font_viewer,
        &_application_information_blespam,
        &_application_information_analogtv,
        &_application_information_nrf_rx,
        &_application_information_coasterp,
        &_application_information_lge,
        &_application_information_lcr,
        &_application_information_jammer,
        &_application_information_gpssim,
        &_application_information_spainter,
        &_application_information_keyfob,
        &_application_information_tetris,
        &_application_information_extsensors,
        &_application_information_foxhunt_rx,
        &_application_information_audio_test,
        &_application_information_wardrivemap,
        &_application_information_tpmsrx,
        &_application_information_protoview,
        &_application_information_adsbtx,
        &_application_information_sstvtx,
        &_application_information_sstvrx,
        &_application_information_random_password,
        &_application_information_wefax_rx,
        &_application_information_noaaapt_rx,
        &_application_information_shoppingcart_lock,
        &_application_information_ookbrute,
        &_application_information_ook_editor,
        &_application_information_cvs_spam,
        &_application_information_flippertx,
        &_application_information_remote,
        &_application_information_mcu_temperature,
        &_application_information_fmradio,
        &_application_information_tuner,
        &_application_information_metronome,
        &_application_information_app_manager,
        &_application_information_hopper,
        &_application_information_antenna_length,
        &_application_information_view_wav,
        &_application_information_sd_wipe,
        &_application_information_playlist_editor,
        &_application_information_snake,
        &_application_information_stopwatch,
        &_application_information_breakout,
        &_application_information_dinogame,
        &_application_information_doom,
        &_application_information_debug_pmem,
        &_application_information_scanner,
        &_application_information_level,
        &_application_information_gfxeq,
        &_application_information_waterfall_designer,
        &_application_information_detector_rx,
        &_application_information_spaceinv,
        &_application_information_blackjack,
        &_application_information_battleship,
        &_application_information_ert,
        &_application_information_epirb_rx,
        &_application_information_soundboard,
        &_application_information_game2048,
        &_application_information_bht_tx,
        &_application_information_morse_practice,
        &_application_information_adult_toys_controller,
        &_application_information_flex_rx,
        &_application_information_subcarrx,
        &_application_information_siggen,
        &_application_information_sdusb,
        &_application_information_morse_radio,
        &_application_information_morseradiotx,
        &_application_information_dtmf_tone,
        &_application_information_audio_spectrum,
    };
    return apps;
}

} // namespace shim
