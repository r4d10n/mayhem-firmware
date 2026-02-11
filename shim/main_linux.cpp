/*
 * Mayhem Firmware — Linux Entry Point
 * Replaces firmware/application/main.cpp for the Linux shim build.
 *
 * Sets up the shim environment:
 *   1. Initialize ChibiOS shim (pthreads)
 *   2. Initialize filesystem (POSIX directory as SD card)
 *   3. Initialize display (WebSocket framebuffer)
 *   4. Initialize input (WebSocket events)
 *   5. Run the PortaPack event loop
 */

#include "ch.h"
#include "hal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <csignal>
#include <sys/stat.h>
#include <sys/mman.h>

/* Firmware includes */
#include "portapack.hpp"
#include "portapack_shared_memory.hpp"
#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_painter.hpp"
#include "ui_navigation.hpp"
#include "event_m0.hpp"
#include "message_queue.hpp"
#include "irq_controls.hpp"
#include "sd_card.hpp"
#include "theme.hpp"

/* SDL2 display backend */
#include "sdl2_backend.hpp"

/* SDL2 audio output */
#include "audio_sink.hpp"

/* SDR backend */
#include "sdr_interface.hpp"
#include "active_sdr.hpp"

/* WebSocket Web UI */
#include "web_ui_server.hpp"

/* Shim-specific declarations */
extern "C" void fatfs_shim_set_root(const char* root);

/* Forward declarations for stubs we need */
extern "C" void lcd_frame_sync_configure(void);
extern "C" void rtc_interrupt_enable(void);

/* Global pointer for compatibility with firmware code */
ui::SystemView* system_view_ptr = nullptr;

/* screen_width/screen_height defined in firmware's ui.cpp */

/* ---- Signal handling ---- */

static volatile bool running = true;

static void signal_handler(int sig) {
    (void)sig;
    fprintf(stderr, "\n[SHIM] Signal received, shutting down...\n");
    running = false;
    EventDispatcher::request_stop();
}

/* ---- SD card directory setup ---- */

static void ensure_sdcard_directories(const std::string& root) {
    const char* dirs[] = {
        "/APPS",
        "/CAPTURES",
        "/FIRMWARE",
        "/FREQMAN",
        "/LOGS",
        "/LOOKING_GLASS",
        "/SCREENSHOTS",
        "/SETTINGS",
        "/SPLASH",
        nullptr
    };

    /* Create root if needed */
    mkdir(root.c_str(), 0755);

    for (const char** d = dirs; *d; d++) {
        std::string path = root + *d;
        mkdir(path.c_str(), 0755);
    }
}

/* ---- Usage ---- */

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Mayhem Firmware — Linux Shim\n"
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --sdcard-root PATH   SD card directory (default: ~/.portapack)\n"
        "  --web-port PORT      Web UI port (default: 8080)\n"
        "  --no-sdr             Run without SDR hardware\n"
        "  --no-audio           Run without audio output\n"
        "  --sdr-driver DRIVER  SoapySDR driver name (e.g. hackrf, rtlsdr)\n"
        "  --sdr-serial SERIAL  SDR device serial number\n"
        "  --sdr-backend NAME   SDR backend: soapy (default), libiio, plutosdr\n"
        "  --verbose            Enable debug logging\n"
        "  --help               Show this help\n",
        prog);
}

/* ---- Main ---- */

int main(int argc, char* argv[]) {
    /* Default configuration */
    std::string sdcard_root;
    int web_port = 8080;
    bool verbose = false;
    bool no_audio = false;
    bool no_sdr = false;
    std::string sdr_driver;
    std::string sdr_serial;
    std::string sdr_backend = "soapy";

    /* Default SD card root: ~/.portapack */
    const char* home = getenv("HOME");
    if (home) {
        sdcard_root = std::string(home) + "/.portapack";
    } else {
        sdcard_root = "/tmp/portapack";
    }

    /* Parse command line */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sdcard-root") == 0 && i + 1 < argc) {
            sdcard_root = argv[++i];
        } else if (strcmp(argv[i], "--web-port") == 0 && i + 1 < argc) {
            web_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-audio") == 0) {
            no_audio = true;
        } else if (strcmp(argv[i], "--no-sdr") == 0) {
            no_sdr = true;
        } else if (strcmp(argv[i], "--sdr-driver") == 0 && i + 1 < argc) {
            sdr_driver = argv[++i];
        } else if (strcmp(argv[i], "--sdr-serial") == 0 && i + 1 < argc) {
            sdr_serial = argv[++i];
        } else if (strcmp(argv[i], "--sdr-backend") == 0 && i + 1 < argc) {
            sdr_backend = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* web_port is used by WebUIServer below */

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Banner */
    fprintf(stderr,
        "=== Mayhem Firmware Linux Shim ===\n"
        "SD card root:  %s\n"
        "Web UI port:   %d\n"
        "SDR backend:   %s\n"
        "SDR driver:    %s\n"
        "SDR serial:    %s\n"
        "SDR enabled:   %s\n"
        "Verbose:       %s\n"
        "==================================\n",
        sdcard_root.c_str(), web_port,
        sdr_backend.c_str(),
        sdr_driver.empty() ? "(auto)" : sdr_driver.c_str(),
        sdr_serial.empty() ? "(any)" : sdr_serial.c_str(),
        no_sdr ? "no" : "yes",
        verbose ? "yes" : "no");

    /* Map LPC43xx backup RAM region at its hardware address.
     * The persistent_memory module stores settings here via a pointer
     * derived from LPC_BACKUP_REG_BASE (0x40041000). On Linux this
     * address is unmapped, so we mmap a page to make it valid. */
    {
        const uintptr_t backup_page = 0x40041000u & ~0xFFFu;  /* page-align */
        void* p = mmap(reinterpret_cast<void*>(backup_page), 4096,
                       PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED_NOREPLACE,
                       -1, 0);
        if (p == MAP_FAILED) {
            /* Fall back to MAP_FIXED if MAP_FIXED_NOREPLACE unavailable */
            p = mmap(reinterpret_cast<void*>(backup_page), 4096,
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,
                     -1, 0);
        }
        if (p == MAP_FAILED) {
            fprintf(stderr, "[SHIM] Warning: mmap backup RAM failed\n");
        }
    }

    /* Initialize ChibiOS shim */
    chSysInit();

    /* Set up SD card filesystem */
    ensure_sdcard_directories(sdcard_root);
    fatfs_shim_set_root(sdcard_root.c_str());

    /* Initialize HAL stubs */
    sdcStart(&SDCD1, nullptr);

    /* Initialize theme with default */
    Theme::SetTheme(Theme::ThemeId::DefaultGrey);

    /* Initialize SDL2 display backend */
    shim::SDL2Backend::get().init(240, 320, 2);
    shim::SDL2Backend::get().start();

    /* Initialize SDL2 audio output */
    if (!no_audio) {
        if (shim::AudioSink::get().init(48000, 1024)) {
            fprintf(stderr, "[AudioSink] Audio output enabled\n");
        } else {
            fprintf(stderr, "[AudioSink] Audio output failed — continuing without audio\n");
        }
    }

    /* Initialize Web UI server */
    shim::WebUIServer::get().start(web_port);

    /* Initialize SDR device */
    std::unique_ptr<shim::SDRInterface> sdr_device;
    if (!no_sdr) {
        // Build device_args string from driver/serial
        std::string device_args;
        if (!sdr_driver.empty()) {
            device_args += "driver=" + sdr_driver;
        }
        if (!sdr_serial.empty()) {
            if (!device_args.empty()) device_args += ",";
            device_args += "serial=" + sdr_serial;
        }

        sdr_device = shim::SDRInterface::create(sdr_backend, device_args);
        if (sdr_device) {
            shim::set_active_sdr(sdr_device.get());
            fprintf(stderr, "[SDR] Connected to %s (%s)\n",
                    sdr_device->get_hardware_name().c_str(),
                    sdr_device->get_driver_name().c_str());
        } else {
            fprintf(stderr, "[SDR] No SDR device found — running without radio\n");
        }
    }

    fprintf(stderr, "[SHIM] Starting event loop...\n");

    /* Create UI context and system view */
    static ui::Context context;
    static ui::SystemView system_view{
        context,
        portapack::display.screen_rect()};

    system_view_ptr = &system_view;

    /* Create and run event dispatcher */
    EventDispatcher event_dispatcher{&system_view, context};

    static MessageHandlerRegistration message_handler_display_sleep{
        Message::ID::DisplaySleep,
        [&event_dispatcher](const Message* const) {
            event_dispatcher.set_display_sleep(true);
        }};

    /* Start the event loop (blocks until shutdown) */
    event_dispatcher.run();

    /* Shutdown Web UI */
    shim::WebUIServer::get().stop();

    /* Shutdown SDR */
    if (sdr_device) {
        shim::set_active_sdr(nullptr);
        sdr_device->close();
        sdr_device.reset();
    }

    /* Shutdown audio */
    shim::AudioSink::get().shutdown();

    /* Shutdown SDL2 backend */
    shim::SDL2Backend::get().stop();

    /* Cleanup */
    sdcDisconnect(&SDCD1);
    sdcStop(&SDCD1);

    fprintf(stderr, "[SHIM] Shutdown complete.\n");
    return 0;
}
