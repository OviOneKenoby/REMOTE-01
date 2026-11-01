#include "artwork/surface.hpp"

#include "esp_heap_caps.h"

#include <algorithm>
#include <cstdlib>

namespace artwork {
namespace {

using Pixel = lv_color_t;

Pixel color(std::uint32_t value) { return lv_color_hex(value); }

void put(Pixel *pixels, int x, int y, Pixel value) {
  if (x >= 0 && x < Surface::width && y >= 0 && y < Surface::height)
    pixels[std::size_t(y) * Surface::width + x] = value;
}

void disc(Pixel *pixels, int cx, int cy, int radius, Pixel value) {
  const int r2 = radius * radius;
  for (int y = cy - radius; y <= cy + radius; ++y)
    for (int x = cx - radius; x <= cx + radius; ++x) {
      const int dx = x - cx, dy = y - cy;
      if (dx * dx + dy * dy <= r2) put(pixels, x, y, value);
    }
}

void ring(Pixel *pixels, int cx, int cy, int radius, int thickness,
          Pixel value) {
  const int outer = radius * radius;
  const int inner = (radius - thickness) * (radius - thickness);
  for (int y = cy - radius; y <= cy + radius; ++y)
    for (int x = cx - radius; x <= cx + radius; ++x) {
      const int dx = x - cx, dy = y - cy, d = dx * dx + dy * dy;
      if (d <= outer && d >= inner) put(pixels, x, y, value);
    }
}

void line(Pixel *pixels, int x0, int y0, int x1, int y1, int thickness,
          Pixel value) {
  const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  for (;;) {
    disc(pixels, x0, y0, thickness / 2, value);
    if (x0 == x1 && y0 == y1) break;
    const int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void rectangle(Pixel *pixels, int x0, int y0, int x1, int y1, int thickness,
               Pixel value) {
  for (int i = 0; i < thickness; ++i) {
    line(pixels, x0 + i, y0 + i, x1 - i, y0 + i, 1, value);
    line(pixels, x1 - i, y0 + i, x1 - i, y1 - i, 1, value);
    line(pixels, x1 - i, y1 - i, x0 + i, y1 - i, 1, value);
    line(pixels, x0 + i, y1 - i, x0 + i, y0 + i, 1, value);
  }
}

void radio(Pixel *pixels, Pixel accent, Pixel foreground, Pixel muted) {
  line(pixels, 42, 43, 94, 20, 4, accent);
  rectangle(pixels, 29, 43, 115, 108, 4, foreground);
  rectangle(pixels, 39, 54, 104, 72, 2, muted);
  line(pixels, 48, 63, 89, 63, 3, accent);
  disc(pixels, 93, 63, 5, accent);
  ring(pixels, 52, 90, 14, 4, foreground);
  disc(pixels, 52, 90, 4, muted);
  disc(pixels, 92, 91, 6, accent);
}

void bluetooth(Pixel *pixels, Pixel accent, Pixel foreground) {
  ring(pixels, 72, 72, 48, 3, foreground);
  line(pixels, 72, 31, 72, 113, 5, accent);
  line(pixels, 72, 31, 96, 52, 5, accent);
  line(pixels, 96, 52, 48, 92, 5, accent);
  line(pixels, 48, 52, 96, 92, 5, accent);
  line(pixels, 96, 92, 72, 113, 5, accent);
}

void neutral(Pixel *pixels, Pixel foreground, Pixel muted) {
  ring(pixels, 72, 72, 46, 3, muted);
  line(pixels, 67, 48, 67, 91, 5, foreground);
  line(pixels, 67, 48, 93, 43, 5, foreground);
  line(pixels, 93, 43, 93, 82, 5, foreground);
  disc(pixels, 58, 94, 10, foreground);
  disc(pixels, 84, 85, 10, foreground);
}

void draw(Pixel *pixels, Source source) {
  const auto background = color(0x1c292e);
  const auto foreground = color(0xf3f5f2);
  const auto accent = color(0xc2ff00);
  const auto muted = color(0x71838a);
  std::fill_n(pixels, Surface::pixel_count, background);
  if (source == Source::radio) radio(pixels, accent, foreground, muted);
  else if (source == Source::bluetooth) bluetooth(pixels, accent, foreground);
  else neutral(pixels, foreground, muted);
}

} // namespace

bool Surface::initialize() {
  if (attempted_) return ready();
  attempted_ = true;
  pixels_ = static_cast<lv_color_t *>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!pixels_) return false;
  descriptor_ = {};
  descriptor_.header.always_zero = 0;
  descriptor_.header.w = width;
  descriptor_.header.h = height;
  descriptor_.header.cf = LV_IMG_CF_TRUE_COLOR;
  descriptor_.data_size = bytes;
  descriptor_.data = reinterpret_cast<const std::uint8_t *>(pixels_);
  draw(pixels_, Source::neutral);
  source_ = Source::neutral;
  return true;
}

bool Surface::render(Source source) {
  if (!ready()) return false;
  if (source == source_) return true;
  draw(pixels_, source);
  source_ = source;
  commit_ui_buffer();
  return true;
}

void Surface::commit_ui_buffer() {
  if (ready()) lv_img_cache_invalidate_src(&descriptor_);
}

} // namespace artwork
