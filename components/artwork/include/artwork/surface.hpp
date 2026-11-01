#pragma once

#include "artwork/model.hpp"
#include "lvgl.h"

#include <cstddef>
#include <cstdint>

namespace artwork {

class Surface {
public:
  static constexpr std::uint16_t width = 144;
  static constexpr std::uint16_t height = 144;
  static constexpr std::size_t pixel_count = std::size_t(width) * height;
  static constexpr std::size_t bytes = pixel_count * sizeof(lv_color_t);
  static_assert(sizeof(lv_color_t) == 2);
  static_assert(bytes == 41472);

  // Called by the LVGL owner. The one PSRAM surface is retained until reboot,
  // so a deleted/recreated HOME widget can never outlive its image descriptor.
  bool initialize();
  bool render(Source source);

  bool ready() const { return pixels_ != nullptr; }
  bool allocation_attempted() const { return attempted_; }
  Source source() const { return source_; }
  const lv_img_dsc_t *descriptor() const {
    return ready() ? &descriptor_ : nullptr;
  }

  // Future bounded decoders may write this surface only while owned by the UI
  // task. A later asynchronous milestone must add a separate staging buffer.
  lv_color_t *ui_write_buffer() { return pixels_; }
  void commit_ui_buffer();

private:
  lv_color_t *pixels_{nullptr};
  lv_img_dsc_t descriptor_{};
  Source source_{Source::neutral};
  bool attempted_{false};
};

} // namespace artwork
