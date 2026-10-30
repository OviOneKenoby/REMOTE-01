#pragma once

#include <cstdint>

namespace control_home::navigation {

enum class Page : std::uint8_t {
  HOME,
  CONTROL,
  SAVED,
  FAVORITES,
  RECENT,
  BROWSER_COUNTRY,
  BROWSER_NAME,
  BROWSER_GENRE,
  BROWSER_RESULTS
};

constexpr bool is_browser_page(Page page) {
  return page == Page::BROWSER_COUNTRY || page == Page::BROWSER_NAME ||
         page == Page::BROWSER_GENRE || page == Page::BROWSER_RESULTS;
}

struct BrowserDisconnectRecovery {
  bool return_home = false;

  void disconnected(Page page) { return_home = is_browser_page(page); }
  bool reconnected() {
    const bool result = return_home;
    return_home = false;
    return result;
  }
  void clear() { return_home = false; }
};

struct Model {
  Page page = Page::HOME;
  bool favorites_from_home = false;

  void open_control() { page = Page::CONTROL; favorites_from_home = false; }
  void open_saved() { page = Page::SAVED; }
  void open_favorites(bool from_home) {
    page = Page::FAVORITES;
    favorites_from_home = from_home;
  }
  void open_recent() { page = Page::RECENT; }
  void open_browser() { page = Page::BROWSER_COUNTRY; }
  void browser_next() {
    if (page == Page::BROWSER_COUNTRY) page = Page::BROWSER_NAME;
    else if (page == Page::BROWSER_NAME) page = Page::BROWSER_GENRE;
    else if (page == Page::BROWSER_GENRE) page = Page::BROWSER_RESULTS;
  }
  void back() {
    if (page == Page::HOME) return;
    if (page == Page::CONTROL ||
        (page == Page::FAVORITES && favorites_from_home)) {
      exit();
      return;
    }
    if (page == Page::BROWSER_RESULTS) page = Page::BROWSER_GENRE;
    else if (page == Page::BROWSER_GENRE) page = Page::BROWSER_NAME;
    else if (page == Page::BROWSER_NAME) page = Page::BROWSER_COUNTRY;
    else page = Page::CONTROL;
  }
  void exit() { page = Page::HOME; favorites_from_home = false; }
  void wake() { exit(); }
};

enum class GestureResult : std::uint8_t { NONE, SHORT_TAP, LONG_PRESS };

struct Gesture {
  static constexpr std::uint32_t long_press_ms = 2000;
  static constexpr std::int16_t movement_limit = 8;
  bool active = false;
  bool cancelled = false;
  bool long_emitted = false;
  std::uint32_t started_ms = 0;
  std::int16_t start_x = 0, start_y = 0;

  void press(std::uint32_t now, std::int16_t x, std::int16_t y) {
    active = true; cancelled = false; long_emitted = false;
    started_ms = now; start_x = x; start_y = y;
  }
  void move(std::int16_t x, std::int16_t y) {
    const auto dx = x > start_x ? x - start_x : start_x - x;
    const auto dy = y > start_y ? y - start_y : start_y - y;
    if (dx > movement_limit || dy > movement_limit) cancelled = true;
  }
  void cancel() { cancelled = true; }
  GestureResult hold(std::uint32_t now) {
    if (!active || cancelled || long_emitted || now - started_ms < long_press_ms)
      return GestureResult::NONE;
    long_emitted = true;
    return GestureResult::LONG_PRESS;
  }
  GestureResult release(std::uint32_t now) {
    const auto held = hold(now);
    const bool short_tap = active && !cancelled && !long_emitted;
    active = false;
    if (held == GestureResult::LONG_PRESS) return held;
    return short_tap ? GestureResult::SHORT_TAP : GestureResult::NONE;
  }
};

}  // namespace control_home::navigation
