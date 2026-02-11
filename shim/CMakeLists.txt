cmake_minimum_required(VERSION 3.16)
project(portapack-linux CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ---- Root paths ----
set(FW_ROOT ${CMAKE_SOURCE_DIR}/../firmware)
set(APP_DIR ${FW_ROOT}/application)
set(COMMON  ${FW_ROOT}/common)
set(BB_DIR  ${FW_ROOT}/baseband)
set(SHIM    ${CMAKE_SOURCE_DIR})

# ---- Include paths ----
# CRITICAL: shim/include goes FIRST to override ch.h, hal.h, ff.h, chprintf.h
# from ChibiOS. The -I ordering is what makes the shim work.
include_directories(BEFORE
    ${SHIM}/include
)

include_directories(
    ${SHIM}/display
    ${SHIM}/platform
    ${SHIM}/sdr
    ${BB_DIR}
    ${APP_DIR}
    ${APP_DIR}/ui
    ${APP_DIR}/apps
    ${APP_DIR}/hw
    ${APP_DIR}/protocols
    ${APP_DIR}/bmps
    ${COMMON}
    ${FW_ROOT}
    # LPC43xx register type definitions (lpc43xx.inc)
    ${FW_ROOT}/chibios-portapack/os/hal/platforms/LPC43xx
)

# ---- Compiler definitions ----
add_definitions(
    -DLPC43XX
    -DLPC43XX_M0
    -D__NEWLIB__
    -DHACKRF_ONE
    -DTOOLCHAIN_GCC
    -D_RANDOM_TCC=0
    -DLINUX_SHIM=1
    -DVERSION_STRING="linux-shim"
    -DVERSION_MD5=0
    -DGCC_VERSION_MISMATCH=0
    -DFLASH_SIZE_MB=2
    -DFLASH_SIZE_LIMIT_MB=1
)

# ---- Compiler flags ----
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-sign-compare -Wno-volatile -Wno-psabi -fpermissive")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -Wno-unused-parameter")

# ---- SDL2 ----
find_package(SDL2 REQUIRED)

# ---- SoapySDR (optional) ----
find_package(SoapySDR CONFIG QUIET)
if(SoapySDR_FOUND)
    message(STATUS "SoapySDR found: ${SoapySDR_VERSION}")
    add_definitions(-DSOAPY_SDR_AVAILABLE=1)
    set(SOAPY_LIBS SoapySDR)
else()
    message(STATUS "SoapySDR not found - building without SDR support")
    add_definitions(-DSOAPY_SDR_AVAILABLE=0)
    set(SOAPY_LIBS "")
endif()

# ---- Shim sources ----
set(SHIM_SOURCES
    ${SHIM}/main_linux.cpp
    ${SHIM}/platform/chibios_shim.cpp
    ${SHIM}/platform/hal_shim.cpp
    ${SHIM}/platform/fatfs_shim.cpp
    ${SHIM}/platform/chibios_cpp_shim.cpp
    ${SHIM}/platform/shared_memory_shim.cpp
    ${SHIM}/drivers/hardware_stubs.cpp
    ${SHIM}/display/framebuffer.cpp
    ${SHIM}/display/sdl2_backend.cpp
    ${SHIM}/sdr/audio_sink.cpp
    ${SHIM}/sdr/audio_dma_shim.cpp
    ${SHIM}/sdr/soapy_sdr_shim.cpp
    ${SHIM}/sdr/baseband_stubs.cpp
    ${SHIM}/sdr/processor_factory.cpp
    ${SHIM}/sdr/baseband_thread_shim.cpp
    ${SHIM}/sdr/external_app_registry.cpp
)

# SoapySDR code uses try/catch — enable exceptions for SDR files
set_source_files_properties(
    ${SHIM}/sdr/soapy_sdr_shim.cpp
    PROPERTIES COMPILE_FLAGS "-fexceptions"
)

# ---- Firmware common sources (application logic, not hardware) ----
set(COMMON_SOURCES
    ${COMMON}/adsb.cpp
    ${COMMON}/adsb_frame.cpp
    ${COMMON}/ais_baseband.cpp
    ${COMMON}/ais_packet.cpp
    ${COMMON}/aprs_packet.cpp
    ${COMMON}/bch_code.cpp
    ${COMMON}/bmpfile.cpp
    ${COMMON}/ert_packet.cpp
    ${COMMON}/event.cpp
    ${COMMON}/lfsr_random.cpp
    ${COMMON}/manchester.cpp
    ${COMMON}/message_queue.cpp
    ${COMMON}/morse.cpp
    ${COMMON}/png_writer.cpp
    ${COMMON}/pocsag.cpp
    ${COMMON}/pocsag_packet.cpp
    ${COMMON}/portapack_persistent_memory.cpp
    ${COMMON}/sonde_packet.cpp
    ${COMMON}/tpms_packet.cpp
    ${COMMON}/ui.cpp
    ${COMMON}/ui_focus.cpp
    ${COMMON}/ui_painter.cpp
    ${COMMON}/ui_text.cpp
    ${COMMON}/ui_widget.cpp
    ${COMMON}/ui_language.cpp
    ${COMMON}/utility.cpp
    ${COMMON}/buffer.cpp
    ${COMMON}/dsp_iir.cpp
    ${COMMON}/random.cpp
    ${COMMON}/performance_counter.cpp
    ${COMMON}/backlight.cpp
    ${COMMON}/i2cdevmanager.cpp
    ${COMMON}/i2cdev_max17055.cpp
    ${COMMON}/i2cdev_ppmod.cpp
)

# ---- Application sources ----
set(APP_SOURCES
    ${APP_DIR}/app_settings.cpp
    ${APP_DIR}/database.cpp
    ${APP_DIR}/event_m0.cpp
    ${APP_DIR}/file.cpp
    ${APP_DIR}/file_path.cpp
    ${APP_DIR}/file_reader.cpp
    ${APP_DIR}/freqman_db.cpp
    ${APP_DIR}/freqman.cpp
    ${APP_DIR}/gradient.cpp
    ${APP_DIR}/io_convert.cpp
    ${APP_DIR}/io_file.cpp
    ${APP_DIR}/io_wave.cpp
    ${APP_DIR}/iq_trim.cpp
    ${APP_DIR}/log_file.cpp
    ${APP_DIR}/metadata_file.cpp
    ${APP_DIR}/flipper_subfile.cpp
    ${APP_DIR}/ook_file.cpp
    ${APP_DIR}/qrcodegen.cpp
    ${APP_DIR}/recent_entries.cpp
    ${APP_DIR}/rtc_time.cpp
    ${APP_DIR}/sd_card.cpp
    ${APP_DIR}/serializer.cpp
    ${APP_DIR}/spectrum_color_lut.cpp
    ${APP_DIR}/string_format.cpp
    ${APP_DIR}/theme.cpp
    ${APP_DIR}/tone_key.cpp
    ${APP_DIR}/config_mode.cpp
    ${APP_DIR}/view_factory_base.cpp
    ${APP_DIR}/ui_baseband_stats_view.cpp
    ${APP_DIR}/ui_navigation.cpp
    ${APP_DIR}/ui_record_view.cpp
    ${APP_DIR}/ui_sd_card_status_view.cpp
    # ui_sd_card_debug.cpp excluded — ChibiOS hardware benchmarking, not meaningful on Linux
    ${APP_DIR}/ui_external_items_menu_loader.cpp
    # Additional source files to resolve undefined symbols
    ${APP_DIR}/receiver_model.cpp
    ${APP_DIR}/transmitter_model.cpp
    ${APP_DIR}/baseband_api.cpp
    # debug.cpp excluded — too many hardware-specific dependencies (CORTEX_M0, extctx, etc.)
    ${APP_DIR}/temperature_logger.cpp
    ${APP_DIR}/clock_manager.cpp
    # usb_serial.cpp excluded — requires ChibiOS shell.h
    ${APP_DIR}/replay_thread.cpp
)

# ---- UI sources ----
set(UI_SOURCES
    ${APP_DIR}/ui/ui_alphanum.cpp
    ${APP_DIR}/ui/ui_audio.cpp
    ${APP_DIR}/ui/ui_channel.cpp
    ${APP_DIR}/ui/ui_font_fixed_5x8.cpp
    ${APP_DIR}/ui/ui_font_fixed_8x16.cpp
    ${APP_DIR}/ui/ui_geomap.cpp
    ${APP_DIR}/ui/ui_qrcode.cpp
    ${APP_DIR}/ui/ui_menu.cpp
    ${APP_DIR}/ui/ui_btngrid.cpp
    ${APP_DIR}/ui/ui_receiver.cpp
    ${APP_DIR}/ui/ui_rssi.cpp
    ${APP_DIR}/ui/ui_freqlist.cpp
    ${APP_DIR}/ui/ui_spectrum.cpp
    ${APP_DIR}/ui/ui_tabview.cpp
    ${APP_DIR}/ui/ui_textentry.cpp
    ${APP_DIR}/ui/ui_tone_key.cpp
    ${APP_DIR}/ui/ui_transmitter.cpp
    ${APP_DIR}/ui/ui_bmpview.cpp
)

# ---- App view sources ----
set(APPS_SOURCES
    ${APP_DIR}/apps/ais_app.cpp
    ${APP_DIR}/apps/analog_audio_app.cpp
    ${APP_DIR}/apps/ble_rx_app.cpp
    ${APP_DIR}/apps/ble_tx_app.cpp
    ${APP_DIR}/apps/capture_app.cpp
    ${APP_DIR}/apps/pocsag_app.cpp
    ${APP_DIR}/apps/ui_about_simple.cpp
    ${APP_DIR}/apps/ui_adsb_rx.cpp
    ${APP_DIR}/apps/ui_aprs_rx.cpp
    ${APP_DIR}/apps/ui_aprs_tx.cpp
    ${APP_DIR}/apps/ui_battinfo.cpp
    ${APP_DIR}/apps/ui_bmp_file_viewer.cpp
    ${APP_DIR}/apps/ui_btle_rx.cpp
    ${APP_DIR}/apps/ui_debug.cpp
    ${APP_DIR}/apps/ui_debug_max17055.cpp
    ${APP_DIR}/apps/ui_dfu_menu.cpp
    ${APP_DIR}/apps/ui_encoders.cpp
    ${APP_DIR}/apps/ui_external_module_view.cpp
    ${APP_DIR}/apps/ui_fileman.cpp
    ${APP_DIR}/apps/ui_flash_utility.cpp
    ${APP_DIR}/apps/ui_freqman.cpp
    ${APP_DIR}/apps/ui_iq_trim.cpp
    ${APP_DIR}/apps/ui_looking_glass_app.cpp
    ${APP_DIR}/apps/ui_mictx.cpp
    ${APP_DIR}/apps/ui_modemsetup.cpp
    ${APP_DIR}/apps/ui_playlist.cpp
    ${APP_DIR}/apps/ui_pocsag_tx.cpp
    ${APP_DIR}/apps/ui_rds.cpp
    ${APP_DIR}/apps/ui_recon_settings.cpp
    ${APP_DIR}/apps/ui_recon.cpp
    ${APP_DIR}/apps/ui_search.cpp
    ${APP_DIR}/apps/ui_settings.cpp
    ${APP_DIR}/apps/ui_sonde.cpp
    ${APP_DIR}/apps/ui_ss_viewer.cpp
    ${APP_DIR}/apps/ui_standalone_view.cpp
    ${APP_DIR}/apps/ui_subghzd.cpp
    ${APP_DIR}/apps/ui_text_editor.cpp
    ${APP_DIR}/apps/ui_touch_calibration.cpp
    ${APP_DIR}/apps/ui_touchtunes.cpp
    ${APP_DIR}/apps/ui_weatherstation.cpp
)

# ---- Protocol sources ----
set(PROTOCOL_SOURCES
    ${APP_DIR}/protocols/aprs.cpp
    ${APP_DIR}/protocols/ax25.cpp
    ${APP_DIR}/protocols/dcs.cpp
    ${APP_DIR}/protocols/encoders.cpp
    ${APP_DIR}/protocols/modems.cpp
    ${APP_DIR}/protocols/rds.cpp
)

# ---- Baseband DSP sources (Phase 4) ----
# Core DSP and audio processing — these are the computational heart.
# We compile these for x86_64 using our ARM intrinsics shim.
set(BASEBAND_DSP_SOURCES
    ${BB_DIR}/baseband_processor.cpp
    ${BB_DIR}/dsp_decimate.cpp
    ${BB_DIR}/dsp_demodulate.cpp
    ${BB_DIR}/dsp_modulate.cpp
    # dsp_fir_taps.cpp, dsp_iir.cpp, dsp_iir_config.cpp — header-only
    ${BB_DIR}/dsp_squelch.cpp
    ${BB_DIR}/dsp_hilbert.cpp
    ${BB_DIR}/dsp_goertzel.cpp
    ${BB_DIR}/audio_output.cpp
    ${BB_DIR}/audio_input.cpp
    ${BB_DIR}/audio_compressor.cpp
    ${BB_DIR}/audio_stats_collector.cpp
    ${BB_DIR}/channel_decimator.cpp
    # channel_stats_collector.cpp — header-only
    ${BB_DIR}/spectrum_collector.cpp
    ${BB_DIR}/stream_input.cpp
    ${BB_DIR}/stream_output.cpp
    ${BB_DIR}/tone_gen.cpp
    ${BB_DIR}/fxpt_atan2.cpp
    ${BB_DIR}/rssi.cpp
    # rssi_stats_collector.cpp — header-only
    ${BB_DIR}/baseband_stats_collector.cpp
    ${BB_DIR}/matched_filter.cpp
    ${BB_DIR}/tv_collector.cpp
)

# Baseband processor sources — each implements one demodulator/modulator.
# Their main() functions are suppressed via baseband_main_guard.h.
set(BASEBAND_PROC_SOURCES
    ${BB_DIR}/proc_acars.cpp
    ${BB_DIR}/proc_adsbrx.cpp
    ${BB_DIR}/proc_adsbtx.cpp
    ${BB_DIR}/proc_afsk.cpp
    ${BB_DIR}/proc_afskrx.cpp
    ${BB_DIR}/proc_ais.cpp
    ${BB_DIR}/proc_am_audio.cpp
    # proc_am_tv.cpp excluded — redefines WidebandFMAudio (same class name as proc_wfm_audio)
    ${BB_DIR}/proc_aprsrx.cpp
    ${BB_DIR}/proc_audio_beep.cpp
    ${BB_DIR}/proc_audiotx.cpp
    ${BB_DIR}/proc_ble_tx.cpp
    ${BB_DIR}/proc_bint_stream_tx.cpp
    ${BB_DIR}/proc_btlerx.cpp
    ${BB_DIR}/proc_capture.cpp
    ${BB_DIR}/proc_epirb.cpp
    ${BB_DIR}/proc_ert.cpp
    ${BB_DIR}/proc_flex.cpp
    ${BB_DIR}/proc_flash_utility.cpp
    ${BB_DIR}/proc_fsk.cpp
    ${BB_DIR}/proc_fsk_rx.cpp
    ${BB_DIR}/proc_gps_sim.cpp
    ${BB_DIR}/proc_jammer.cpp
    ${BB_DIR}/proc_mictx.cpp
    ${BB_DIR}/proc_morse.cpp
    ${BB_DIR}/proc_morsetx.cpp
    ${BB_DIR}/proc_nfm_audio.cpp
    ${BB_DIR}/proc_noaaapt_rx.cpp
    ${BB_DIR}/proc_nrfrx.cpp
    ${BB_DIR}/proc_ook.cpp
    ${BB_DIR}/proc_pocsag2.cpp
    ${BB_DIR}/proc_protoview.cpp
    ${BB_DIR}/proc_rds.cpp
    ${BB_DIR}/proc_replay.cpp
    ${BB_DIR}/proc_siggen.cpp
    ${BB_DIR}/proc_sonde.cpp
    ${BB_DIR}/proc_spectrum_painter.cpp
    ${BB_DIR}/proc_sstvrx.cpp
    ${BB_DIR}/proc_sstvtx.cpp
    ${BB_DIR}/proc_subcar.cpp
    ${BB_DIR}/proc_subghzd.cpp
    ${BB_DIR}/proc_test.cpp
    ${BB_DIR}/proc_tones.cpp
    ${BB_DIR}/proc_tpms.cpp
    ${BB_DIR}/proc_weather.cpp
    ${BB_DIR}/proc_wefaxrx.cpp
    ${BB_DIR}/proc_wfm_audio.cpp
    ${BB_DIR}/proc_wideband_spectrum.cpp
)

# Suppress the proc_*.cpp int main() functions — each file defines one
# that creates a processor and runs the M4 event loop. On Linux, the
# processor factory handles this instead.
set_source_files_properties(${BASEBAND_PROC_SOURCES}
    PROPERTIES COMPILE_FLAGS "-include ${SHIM}/include/baseband_main_guard.h"
)

# Files we DON'T compile (M4 hardware specific):
#   baseband_thread.cpp — SGPIO/DMA hardware, replaced by shim baseband thread
#   event_m4.cpp — LPC interrupt handler + creg registers
#   rssi_thread.cpp — hardware RSSI sampling
#   audio_dma.cpp — hardware DMA buffers, replaced by audio_dma_shim.cpp
#   sd_over_usb/proc_sd_over_usb.cpp — USB specific

# ---- External app sources (compiled into binary on Linux) ----
set(EXT_DIR ${APP_DIR}/external)

# Add all external app subdirectories to include path
# Each app has its own headers (ui_*.hpp) in its directory
file(GLOB EXT_APP_SUBDIRS LIST_DIRECTORIES true ${EXT_DIR}/*)
foreach(dir ${EXT_APP_SUBDIRS})
    if(IS_DIRECTORY ${dir})
        include_directories(${dir})
    endif()
endforeach()

set(EXTERNAL_APP_SOURCES
    # Each app: main.cpp + ui/view source files
    ${EXT_DIR}/afsk_rx/main.cpp
    ${EXT_DIR}/afsk_rx/ui_afsk_rx.cpp
    ${EXT_DIR}/calculator/main.cpp
    ${EXT_DIR}/calculator/ui_calculator.cpp
    ${EXT_DIR}/font_viewer/main.cpp
    ${EXT_DIR}/font_viewer/ui_font_viewer.cpp
    ${EXT_DIR}/blespam/main.cpp
    ${EXT_DIR}/blespam/ui_blespam.cpp
    ${EXT_DIR}/analogtv/main.cpp
    ${EXT_DIR}/analogtv/analog_tv_app.cpp
    ${EXT_DIR}/analogtv/ui_tv.cpp
    ${EXT_DIR}/nrf_rx/main.cpp
    ${EXT_DIR}/nrf_rx/ui_nrf_rx.cpp
    ${EXT_DIR}/coasterp/main.cpp
    ${EXT_DIR}/coasterp/ui_coasterp.cpp
    ${EXT_DIR}/lge/main.cpp
    ${EXT_DIR}/lge/lge_app.cpp
    ${EXT_DIR}/lge/rfm69.cpp
    ${EXT_DIR}/lcr/main.cpp
    ${EXT_DIR}/lcr/ui_lcr.cpp
    ${EXT_DIR}/jammer/main.cpp
    ${EXT_DIR}/jammer/ui_jammer.cpp
    ${EXT_DIR}/gpssim/main.cpp
    ${EXT_DIR}/gpssim/gps_sim_app.cpp
    ${EXT_DIR}/spainter/main.cpp
    ${EXT_DIR}/spainter/ui_spectrum_painter.cpp
    ${EXT_DIR}/spainter/ui_spectrum_painter_text.cpp
    ${EXT_DIR}/spainter/ui_spectrum_painter_image.cpp
    ${EXT_DIR}/keyfob/main.cpp
    ${EXT_DIR}/keyfob/ui_keyfob.cpp
    ${EXT_DIR}/tetris/main.cpp
    ${EXT_DIR}/tetris/ui_tetris.cpp
    ${EXT_DIR}/extsensors/main.cpp
    ${EXT_DIR}/extsensors/ui_extsensors.cpp
    ${EXT_DIR}/foxhunt/main.cpp
    ${EXT_DIR}/foxhunt/ui_foxhunt_rx.cpp
    ${EXT_DIR}/audio_test/main.cpp
    ${EXT_DIR}/audio_test/ui_audio_test.cpp
    ${EXT_DIR}/wardrivemap/main.cpp
    ${EXT_DIR}/wardrivemap/ui_wardrivemap.cpp
    ${EXT_DIR}/tpmsrx/main.cpp
    ${EXT_DIR}/tpmsrx/tpms_app.cpp
    ${EXT_DIR}/adsbtx/main.cpp
    ${EXT_DIR}/adsbtx/ui_adsb_tx.cpp
    ${EXT_DIR}/protoview/main.cpp
    ${EXT_DIR}/protoview/ui_protoview.cpp
    ${EXT_DIR}/sstvtx/main.cpp
    ${EXT_DIR}/sstvtx/ui_sstvtx.cpp
    ${EXT_DIR}/sstvrx/main.cpp
    ${EXT_DIR}/sstvrx/ui_sstvrx.cpp
    ${EXT_DIR}/random_password/main.cpp
    ${EXT_DIR}/random_password/ui_random_password.cpp
    ${EXT_DIR}/random_password/sha512.cpp
    ${EXT_DIR}/wefax_rx/main.cpp
    ${EXT_DIR}/wefax_rx/ui_wefax_rx.cpp
    ${EXT_DIR}/noaaapt_rx/main.cpp
    ${EXT_DIR}/noaaapt_rx/ui_noaaapt_rx.cpp
    ${EXT_DIR}/shoppingcart_lock/main.cpp
    ${EXT_DIR}/shoppingcart_lock/shoppingcart_lock.cpp
    ${EXT_DIR}/ookbrute/main.cpp
    ${EXT_DIR}/ookbrute/ui_ookbrute.cpp
    ${EXT_DIR}/ook_editor/main.cpp
    ${EXT_DIR}/ook_editor/ui_ook_editor.cpp
    ${EXT_DIR}/cvs_spam/main.cpp
    ${EXT_DIR}/cvs_spam/cvs_spam.cpp
    ${EXT_DIR}/flippertx/main.cpp
    ${EXT_DIR}/flippertx/ui_flippertx.cpp
    ${EXT_DIR}/remote/main.cpp
    ${EXT_DIR}/remote/ui_remote.cpp
    ${EXT_DIR}/mcu_temperature/main.cpp
    ${EXT_DIR}/mcu_temperature/mcu_temperature.cpp
    ${EXT_DIR}/fmradio/main.cpp
    ${EXT_DIR}/fmradio/ui_fmradio.cpp
    ${EXT_DIR}/tuner/main.cpp
    ${EXT_DIR}/tuner/ui_tuner.cpp
    ${EXT_DIR}/metronome/main.cpp
    ${EXT_DIR}/metronome/ui_metronome.cpp
    ${EXT_DIR}/app_manager/main.cpp
    ${EXT_DIR}/app_manager/ui_app_manager.cpp
    ${EXT_DIR}/hopper/main.cpp
    ${EXT_DIR}/hopper/ui_hopper.cpp
    ${EXT_DIR}/antenna_length/main.cpp
    ${EXT_DIR}/antenna_length/ui_whipcalc.cpp
    ${EXT_DIR}/wav_view/main.cpp
    ${EXT_DIR}/wav_view/ui_view_wav.cpp
    ${EXT_DIR}/sd_wipe/main.cpp
    ${EXT_DIR}/sd_wipe/ui_sd_wipe.cpp
    ${EXT_DIR}/playlist_editor/main.cpp
    ${EXT_DIR}/playlist_editor/ui_playlist_editor.cpp
    ${EXT_DIR}/snake/main.cpp
    ${EXT_DIR}/snake/ui_snake.cpp
    ${EXT_DIR}/stopwatch/main.cpp
    ${EXT_DIR}/stopwatch/ui_stopwatch.cpp
    ${EXT_DIR}/breakout/main.cpp
    ${EXT_DIR}/breakout/ui_breakout.cpp
    ${EXT_DIR}/dinogame/main.cpp
    ${EXT_DIR}/dinogame/ui_dinogame.cpp
    ${EXT_DIR}/doom/main.cpp
    ${EXT_DIR}/doom/ui_doom.cpp
    ${EXT_DIR}/debug_pmem/main.cpp
    ${EXT_DIR}/debug_pmem/ui_debug_pmem.cpp
    ${EXT_DIR}/scanner/main.cpp
    ${EXT_DIR}/scanner/ui_scanner.cpp
    ${EXT_DIR}/level/main.cpp
    ${EXT_DIR}/level/ui_level.cpp
    ${EXT_DIR}/gfxeq/main.cpp
    ${EXT_DIR}/gfxeq/ui_gfxeq.cpp
    ${EXT_DIR}/waterfall_designer/main.cpp
    ${EXT_DIR}/waterfall_designer/ui_waterfall_designer.cpp
    ${EXT_DIR}/detector_rx/main.cpp
    ${EXT_DIR}/detector_rx/ui_detector_rx.cpp
    ${EXT_DIR}/spaceinv/main.cpp
    ${EXT_DIR}/spaceinv/ui_spaceinv.cpp
    ${EXT_DIR}/blackjack/main.cpp
    ${EXT_DIR}/blackjack/ui_blackjack.cpp
    ${EXT_DIR}/battleship/main.cpp
    ${EXT_DIR}/battleship/ui_battleship.cpp
    ${EXT_DIR}/ert/main.cpp
    ${EXT_DIR}/ert/ert_app.cpp
    ${EXT_DIR}/epirb_rx/main.cpp
    ${EXT_DIR}/epirb_rx/ui_epirb_rx.cpp
    ${EXT_DIR}/soundboard/main.cpp
    ${EXT_DIR}/soundboard/soundboard_app.cpp
    ${EXT_DIR}/game2048/main.cpp
    ${EXT_DIR}/game2048/ui_game2048.cpp
    ${EXT_DIR}/bht_tx/main.cpp
    ${EXT_DIR}/bht_tx/ui_bht_tx.cpp
    ${EXT_DIR}/bht_tx/bht.cpp
    ${EXT_DIR}/morse_practice/main.cpp
    ${EXT_DIR}/morse_practice/ui_morse_practice.cpp
    ${EXT_DIR}/adult_toys_controller/main.cpp
    ${EXT_DIR}/adult_toys_controller/ui_adult_toys_controller.cpp
    ${EXT_DIR}/flex_rx/main.cpp
    ${EXT_DIR}/flex_rx/ui_flex_rx.cpp
    ${EXT_DIR}/subcarrx/main.cpp
    ${EXT_DIR}/subcarrx/ui_subcar.cpp
    ${EXT_DIR}/siggen/main.cpp
    ${EXT_DIR}/siggen/ui_siggen.cpp
    ${EXT_DIR}/sdusb/main.cpp
    ${EXT_DIR}/sdusb/ui_sd_over_usb.cpp
    ${EXT_DIR}/morse_radio/main.cpp
    ${EXT_DIR}/morse_radio/ui_morse_radio.cpp
    ${EXT_DIR}/morseradiotx/main.cpp
    ${EXT_DIR}/morseradiotx/ui_morse_radiotx.cpp
)

# ---- Build the executable ----
add_executable(portapack-linux
    ${SHIM_SOURCES}
    ${COMMON_SOURCES}
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${APPS_SOURCES}
    ${PROTOCOL_SOURCES}
    ${BASEBAND_DSP_SOURCES}
    ${BASEBAND_PROC_SOURCES}
    ${EXTERNAL_APP_SOURCES}
)

if (TARGET SDL2::SDL2)
    target_link_libraries(portapack-linux SDL2::SDL2 pthread ${SOAPY_LIBS})
else()
    target_include_directories(portapack-linux PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(portapack-linux ${SDL2_LIBRARIES} pthread ${SOAPY_LIBS})
endif()

# Print a summary
message(STATUS "=== Mayhem Linux Shim Build ===")
message(STATUS "Firmware root: ${FW_ROOT}")
message(STATUS "Shim root:     ${SHIM}")
message(STATUS "Baseband procs: ${BASEBAND_PROC_SOURCES}")
message(STATUS "===============================")
