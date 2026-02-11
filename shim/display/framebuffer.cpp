/*
 * Framebuffer drawing primitives for the Linux shim.
 */

#include "framebuffer.hpp"

#include <algorithm>
#include <cstdlib>

namespace shim {

void Framebuffer::fill_rect(int x, int y, int w, int h, uint16_t color) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(WIDTH, x + w);
    int y1 = std::min(HEIGHT, y + h);

    for (int row = y0; row < y1; row++) {
        uint16_t* dst = &pixels_[row * WIDTH + x0];
        int count = x1 - x0;
        for (int i = 0; i < count; i++)
            dst[i] = color;
    }
}

void Framebuffer::copy_pixels(int x, int y, int w, int h, const uint16_t* src) {
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= HEIGHT) {
            src += w;
            continue;
        }
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx >= 0 && dx < WIDTH)
                pixels_[dy * WIDTH + dx] = src[col];
        }
        src += w;
    }
}

void Framebuffer::read_pixels(int x, int y, int w, int h, uint16_t* dst) const {
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx >= 0 && dx < WIDTH && dy >= 0 && dy < HEIGHT)
                *dst++ = pixels_[dy * WIDTH + dx];
            else
                *dst++ = 0;
        }
    }
}

void Framebuffer::draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    /* Bresenham's line algorithm */
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        set_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Framebuffer::fill_circle(int cx, int cy, int radius, uint16_t fg, uint16_t bg) {
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            uint16_t color = (x * x + y * y <= r2) ? fg : bg;
            set_pixel(cx + x, cy + y, color);
        }
    }
}

void Framebuffer::draw_bitmap(int x, int y, int w, int h, const uint8_t* data,
                              uint16_t fg, uint16_t bg, uint8_t zoom) {
    /* 1bpp bitmap expansion: flat bit stream, LSB first.
     * Matches firmware's ILI9341::draw_bitmap which uses:
     *   pixels[i >> 3] & (1U << (i & 0x7))
     * where i is a flat index across all w*h pixels. */
    const int count = w * h;
    for (int i = 0; i < count; i++) {
        int col = i % w;
        int row = i / w;
        uint16_t color = (data[i >> 3] & (1U << (i & 0x7))) ? fg : bg;

        if (zoom <= 1) {
            set_pixel(x + col, y + row, color);
        } else {
            for (int zy = 0; zy < zoom; zy++)
                for (int zx = 0; zx < zoom; zx++)
                    set_pixel(x + col * zoom + zx,
                              y + row * zoom + zy, color);
        }
    }
}

}  // namespace shim
