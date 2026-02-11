cmake_minimum_required(VERSION 3.16)
project(portapack-linux CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ---- Root paths ----
set(FW_ROOT ${CMAKE_SOURCE_DIR}/../firmware)
set(APP_DIR ${FW_ROOT}/application)
set(COMMON  ${FW_ROOT}/common)
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

# ---- Build the executable ----
add_executable(portapack-linux
    ${SHIM_SOURCES}
    ${COMMON_SOURCES}
    ${APP_SOURCES}
    ${UI_SOURCES}
    ${APPS_SOURCES}
    ${PROTOCOL_SOURCES}
)

if (TARGET SDL2::SDL2)
    target_link_libraries(portapack-linux SDL2::SDL2 pthread)
else()
    target_include_directories(portapack-linux PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(portapack-linux ${SDL2_LIBRARIES} pthread)
endif()

# Print a summary
message(STATUS "=== Mayhem Linux Shim Build ===")
message(STATUS "Firmware root: ${FW_ROOT}")
message(STATUS "Shim root:     ${SHIM}")
message(STATUS "===============================")
