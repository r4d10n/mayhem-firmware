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
#include <csignal>
#include <sys/stat.h>

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
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    (void)web_port; /* Will be used by display shim */

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Banner */
    fprintf(stderr,
        "=== Mayhem Firmware Linux Shim ===\n"
        "SD card root: %s\n"
        "Web UI port:  %d\n"
        "Verbose:      %s\n"
        "==================================\n",
        sdcard_root.c_str(), web_port, verbose ? "yes" : "no");

    /* Initialize ChibiOS shim */
    chSysInit();

    /* Set up SD card filesystem */
    ensure_sdcard_directories(sdcard_root);
    fatfs_shim_set_root(sdcard_root.c_str());

    /* Initialize HAL stubs */
    sdcStart(&SDCD1, nullptr);

    /* Initialize theme with default */
    Theme::SetTheme(Theme::ThemeId::DefaultGrey);

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

    /* Cleanup */
    sdcDisconnect(&SDCD1);
    sdcStop(&SDCD1);

    fprintf(stderr, "[SHIM] Shutdown complete.\n");
    return 0;
}
