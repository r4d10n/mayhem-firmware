/*
 * In-memory RGB565 framebuffer for the Linux shim.
 * All ILI9341 drawing methods write here; the SDL2 backend reads
 * from here to present pixels on screen.
 *
 * No mutex — the main thread writes, the SDL2 thread reads.
 * Occasional tearing is acceptable for a development tool.
 */

#ifndef __SHIM_FRAMEBUFFER_HPP__
#define __SHIM_FRAMEBUFFER_HPP__

#include <cstdint>
#include <cstring>

namespace shim {

class Framebuffer {
   public:
    static constexpr int WIDTH = 240;
    static constexpr int HEIGHT = 320;

    static Framebuffer& get() {
        static Framebuffer instance;
        return instance;
    }

    inline void set_pixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            pixels_[y * WIDTH + x] = color;
    }

    inline uint16_t read_pixel(int x, int y) const {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
            return pixels_[y * WIDTH + x];
        return 0;
    }

    void fill_rect(int x, int y, int w, int h, uint16_t color);
    void copy_pixels(int x, int y, int w, int h, const uint16_t* src);
    void read_pixels(int x, int y, int w, int h, uint16_t* dst) const;
    void draw_line(int x0, int y0, int x1, int y1, uint16_t color);
    void fill_circle(int cx, int cy, int radius, uint16_t fg, uint16_t bg);
    void draw_bitmap(int x, int y, int w, int h, const uint8_t* data,
                     uint16_t fg, uint16_t bg, uint8_t zoom = 1);

    const uint16_t* data() const { return pixels_; }

   private:
    Framebuffer() { memset(pixels_, 0, sizeof(pixels_)); }
    uint16_t pixels_[WIDTH * HEIGHT];
};

}  // namespace shim

#endif /* __SHIM_FRAMEBUFFER_HPP__ */
