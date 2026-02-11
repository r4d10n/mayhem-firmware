/*
 * SDL2 display backend implementation.
 *
 * Thread lifecycle:
 *   start() → pthread_create → render_loop()
 *     render_loop:
 *       1. SDL_Init + create window/renderer/texture
 *       2. Loop at ~30fps:
 *          a. Signal EVT_MASK_LCD_FRAME_SYNC → main thread paints
 *          b. SDL_Delay(5) to let painting happen
 *          c. SDL_UpdateTexture from framebuffer
 *          d. SDL_RenderPresent
 *          e. SDL_PollEvent → map to ChibiOS events
 *       3. Cleanup on exit
 *   stop() → running_ = false → pthread_join
 */

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "sdl2_backend.hpp"
#include "framebuffer.hpp"
#include "web_ui_server.hpp"

/* Firmware event system — provides EventDispatcher::events_flag()
 * and the EVT_MASK_* constants. */
#include "event_m0.hpp"

#include <cstdio>
#include <ctime>

namespace shim {

InputState g_input;

bool SDL2Backend::init(int width, int height, int scale) {
    width_ = width;
    height_ = height;
    scale_ = scale;
    initialized_ = true;
    fprintf(stderr, "[SDL2] Initialized (%dx%d, scale %dx)\n",
            width_, height_, scale_);
    return true;
}

void SDL2Backend::start() {
    if (!initialized_) return;
    running_ = true;
    pthread_create(&thread_, nullptr, thread_entry, this);
}

void SDL2Backend::stop() {
    running_ = false;
    pthread_join(thread_, nullptr);
}

void* SDL2Backend::thread_entry(void* arg) {
    auto* self = static_cast<SDL2Backend*>(arg);
    self->render_loop();
    return nullptr;
}

void SDL2Backend::render_loop() {
    /* Prevent SDL from installing its own signal handlers */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "[SDL2] SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "PortaPack Mayhem",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_ * scale_, height_ * scale_,
        SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "[SDL2] CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        /* Fall back to software renderer */
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "[SDL2] CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    /* SDL_PIXELFORMAT_RGB565 matches our framebuffer format directly */
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        width_, height_);
    if (!texture) {
        fprintf(stderr, "[SDL2] CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    fprintf(stderr, "[SDL2] Window opened (%dx%d, scale %dx)\n",
            width_, height_, scale_);

    int rtc_counter = 0;

    while (running_) {
        uint32_t frame_start = SDL_GetTicks();

        /* 1. Signal frame sync — wakes EventDispatcher to paint widgets */
        EventDispatcher::events_flag(EVT_MASK_LCD_FRAME_SYNC);

        /* Signal RTC tick once per second for status bar updates */
        if (++rtc_counter >= 30) {
            EventDispatcher::events_flag(EVT_MASK_RTC_TICK);
            rtc_counter = 0;
        }

        /* 2. Give the main thread time to paint into the framebuffer */
        SDL_Delay(5);

        /* 3. Copy framebuffer → SDL texture (no conversion needed) */
        SDL_UpdateTexture(texture, nullptr,
                          Framebuffer::get().data(),
                          width_ * sizeof(uint16_t));

        /* 3b. Push frame to WebUI clients (if any are connected) */
        if (WebUIServer::get().is_running()) {
            WebUIServer::get().push_frame(
                Framebuffer::get().data(), width_, height_);
        }

        /* 4. Present — framebuffer is already in correct orientation */
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        /* 5. Process input */
        handle_events();

        /* 6. Frame rate limiting (~30fps = 33ms per frame) */
        uint32_t elapsed = SDL_GetTicks() - frame_start;
        if (elapsed < 33) {
            SDL_Delay(33 - elapsed);
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    fprintf(stderr, "[SDL2] Shutdown complete.\n");
}

void SDL2Backend::save_screenshot() {
    const auto& fb = Framebuffer::get();
    int w = width_;
    int h = height_;

    /* Generate timestamped filename */
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char filename[128];
    snprintf(filename, sizeof(filename),
             "screenshot_%04d%02d%02d_%02d%02d%02d.bmp",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    /* Write BMP file (RGB565 → 24-bit BGR for compatibility) */
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "[SDL2] Screenshot failed: %s\n", filename);
        return;
    }

    int row_bytes = w * 3;
    int pad = (4 - (row_bytes % 4)) % 4;
    int data_size = (row_bytes + pad) * h;
    int file_size = 54 + data_size;

    /* BMP header */
    uint8_t hdr[54] = {};
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = file_size; hdr[3] = file_size >> 8;
    hdr[4] = file_size >> 16; hdr[5] = file_size >> 24;
    hdr[10] = 54; /* pixel data offset */
    hdr[14] = 40; /* DIB header size */
    hdr[18] = w; hdr[19] = w >> 8;
    hdr[22] = h; hdr[23] = h >> 8;
    hdr[26] = 1;  /* planes */
    hdr[28] = 24; /* bits per pixel */
    fwrite(hdr, 1, 54, f);

    /* Pixel data (BMP is bottom-up) */
    uint8_t padding[3] = {0, 0, 0};
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint16_t c = fb.read_pixel(x, y);
            uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (c & 0x1F) * 255 / 31;
            uint8_t bgr[3] = {b, g, r};
            fwrite(bgr, 1, 3, f);
        }
        if (pad) fwrite(padding, 1, pad, f);
    }

    fclose(f);
    fprintf(stderr, "[SDL2] Screenshot saved: %s\n", filename);
}

void SDL2Backend::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                EventDispatcher::request_stop();
                /* Wake main thread so it sees is_running==false */
                EventDispatcher::events_flag(EVT_MASK_LCD_FRAME_SYNC);
                running_ = false;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                if (event.key.repeat) break;

                /* F12 — screenshot (on keydown only) */
                if (event.key.keysym.sym == SDLK_F12 && event.type == SDL_KEYDOWN) {
                    save_screenshot();
                    break;
                }

                uint8_t bit = 0;
                bool valid = true;

                switch (event.key.keysym.sym) {
                    case SDLK_RIGHT:
                        bit = 0;
                        break; /* Switch::Right */
                    case SDLK_LEFT:
                        bit = 1;
                        break; /* Switch::Left */
                    case SDLK_DOWN:
                        bit = 2;
                        break; /* Switch::Down */
                    case SDLK_UP:
                        bit = 3;
                        break; /* Switch::Up */
                    case SDLK_RETURN:
                        bit = 4;
                        break; /* Switch::Sel */
                    case SDLK_ESCAPE:
                        bit = 5;
                        break; /* Switch::Dfu */
                    default:
                        valid = false;
                        break;
                }

                if (valid) {
                    uint8_t mask = 1u << bit;
                    uint8_t current = g_input.switches.load(std::memory_order_relaxed);
                    if (event.type == SDL_KEYDOWN) {
                        g_input.switches.store(current | mask, std::memory_order_relaxed);
                    } else {
                        g_input.switches.store(current & ~mask, std::memory_order_relaxed);
                    }
                    EventDispatcher::events_flag(EVT_MASK_SWITCHES);
                }
                break;
            }

            case SDL_MOUSEWHEEL: {
                if (event.wheel.y > 0) {
                    g_input.encoder.fetch_add(1, std::memory_order_relaxed);
                } else if (event.wheel.y < 0) {
                    g_input.encoder.fetch_sub(1, std::memory_order_relaxed);
                }
                EventDispatcher::events_flag(EVT_MASK_ENCODER);
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int x = event.button.x / scale_;
                    int y = event.button.y / scale_;
                    g_input.touch_x.store(x, std::memory_order_relaxed);
                    g_input.touch_y.store(y, std::memory_order_relaxed);
                    g_input.touch_active.store(true, std::memory_order_relaxed);
                    EventDispatcher::events_flag(EVT_MASK_TOUCH);
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                if (event.motion.state & SDL_BUTTON_LMASK) {
                    int x = event.motion.x / scale_;
                    int y = event.motion.y / scale_;
                    g_input.touch_x.store(x, std::memory_order_relaxed);
                    g_input.touch_y.store(y, std::memory_order_relaxed);
                    EventDispatcher::events_flag(EVT_MASK_TOUCH);
                }
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    g_input.touch_active.store(false, std::memory_order_relaxed);
                    EventDispatcher::events_flag(EVT_MASK_TOUCH);
                }
                break;
            }
        }
    }
}

}  // namespace shim
