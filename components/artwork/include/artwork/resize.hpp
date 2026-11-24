#pragma once
// Box-filter resize of a decoded image to a fixed square output.
// The decoder delivers pixels in blocks; each source pixel is added to the output
// cell that contains it (cell = pixel * out / size) and the sums are averaged at
// the end, so the WHOLE source image is visible in the output. Header-only and
// free of LVGL/ESP types so it can be host-tested.
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace artwork::resize {

constexpr int out_size = 144;
// 16-bit sums are exact while one cell covers at most 15 x 15 source pixels
// (255 * 225 < 65536): the decoder keeps the source below 2 x out_size per side.
constexpr std::size_t accum_values = std::size_t(out_size) * out_size * 3;

struct Accumulator {
  std::uint16_t *sum = nullptr; // accum_values entries, R,G,B per output cell
  int width = 0, height = 0;    // source size
  void clear() { std::fill_n(sum, accum_values, std::uint16_t(0)); }
  // Adds one source pixel. Out-of-range pixels are ignored.
  void add(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    const int dx = x * out_size / width, dy = y * out_size / height;
    auto *cell = sum + (std::size_t(dy) * out_size + std::size_t(dx)) * 3;
    cell[0] = std::uint16_t(cell[0] + r);
    cell[1] = std::uint16_t(cell[1] + g);
    cell[2] = std::uint16_t(cell[2] + b);
  }
  // Averages the cells into out_size * out_size pixels; pack(r, g, b) -> uint16_t.
  // Returns false when the geometry is unusable (source smaller than the output).
  template <class Pack>
  bool finish(std::uint16_t *pixels, Pack pack) const {
    if (width < out_size || height < out_size) return false;
    std::uint16_t columns[out_size]{}, rows[out_size]{};
    for (int x = 0; x < width; ++x) ++columns[x * out_size / width];
    for (int y = 0; y < height; ++y) ++rows[y * out_size / height];
    for (int dy = 0; dy < out_size; ++dy)
      for (int dx = 0; dx < out_size; ++dx) {
        const unsigned count = unsigned(columns[dx]) * rows[dy];
        const auto *cell = sum + (std::size_t(dy) * out_size + std::size_t(dx)) * 3;
        if (!count) { pixels[dy * out_size + dx] = 0; continue; }
        pixels[dy * out_size + dx] =
            pack(std::uint8_t((cell[0] + count / 2) / count),
                 std::uint8_t((cell[1] + count / 2) / count),
                 std::uint8_t((cell[2] + count / 2) / count));
      }
    return true;
  }
};

} // namespace artwork::resize
