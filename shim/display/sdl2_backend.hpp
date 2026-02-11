/*
 * SDL2 display backend for the Linux shim.
 *
 * Runs an SDL2 window in a dedicated thread that:
 *   - Signals EVT_MASK_LCD_FRAME_SYNC at ~30fps to drive the paint cycle
 *   - Copies the framebuffer to an SDL texture for display
 *   - Maps keyboard/mouse input to ChibiOS event signals
 */

#ifndef __SHIM_SDL2_BACKEND_HPP__
#define __SHIM_SDL2_BACKEND_HPP__

#include <cstdint>
#include <atomic>
#include <pthread.h>

namespace shim {

/*
 * Shared input state — written by the SDL2 thread, read by hardware stubs.
 * Single-writer/single-reader, so relaxed atomics suffice.
 */
struct InputState {
    std::atomic<uint8_t> switches{0};
    std::atomic<uint32_t> encoder{0};

    std::atomic<bool> touch_active{false};
    std::atomic<int16_t> touch_x{0};
    std::atomic<int16_t> touch_y{0};
};

extern InputState g_input;

class SDL2Backend {
   public:
    static SDL2Backend& get() {
        static SDL2Backend instance;
        return instance;
    }

    bool init(int width, int height, int scale);
    void start();
    void stop();

   private:
    SDL2Backend() = default;

    static void* thread_entry(void* arg);
    void render_loop();
    void handle_events();
    void save_screenshot();

    int width_ = 240;
    int height_ = 320;
    int scale_ = 2;

    pthread_t thread_{};
    std::atomic<bool> running_{false};
    bool initialized_ = false;
};

}  // namespace shim

#endif /* __SHIM_SDL2_BACKEND_HPP__ */
