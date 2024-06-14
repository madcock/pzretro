#include "graphics.h"

#include <algorithm>
#include <mutex>

namespace graphics {

uint16_t framebuffer[framebuffer_len]{};
uint16_t framebuffer_screen[framebuffer_len]{};
std::mutex mutex;
std::mutex screen_mutex;

void clear()
{
    std::lock_guard<std::mutex> guard(mutex);
    std::fill(framebuffer, framebuffer + framebuffer_len, 0x0080);
}

void fill(int x, int y, int w, int h, uint16_t color)
{
    std::lock_guard<std::mutex> guard(mutex);
    if (x == 0 && y == 0 && w == width && h == height) {
        std::fill(framebuffer, framebuffer + framebuffer_len, color);
    } else {
        for (int i = 0; i < h; i++) {
            std::fill(framebuffer + x + y * stride + i * stride, framebuffer + x + y * stride + i * stride + w, color);
        }
    }
}

void flip()
{
    std::lock_guard<std::mutex> guard1(mutex);
    std::lock_guard<std::mutex> guard2(screen_mutex);
    std::copy(framebuffer, framebuffer + framebuffer_len, framebuffer_screen);
}

} // namespace graphics
