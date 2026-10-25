#include "control_home/full_control_navigation.hpp"

#include <cassert>
#include <cstdio>

using namespace control_home::navigation;

int main() {
  Model model;
  assert(model.page == Page::HOME);
  model.open_control(); model.open_saved(); model.back();
  assert(model.page == Page::CONTROL);
  model.open_favorites(false); model.back();
  assert(model.page == Page::CONTROL);
  model.exit(); model.open_favorites(true); model.back();
  assert(model.page == Page::HOME);
  model.open_control(); model.open_browser(); model.browser_next();
  model.browser_next(); model.browser_next();
  assert(model.page == Page::BROWSER_RESULTS);
  model.back(); model.back(); model.back();
  assert(model.page == Page::BROWSER_COUNTRY);
  model.wake(); assert(model.page == Page::HOME);

  Gesture gesture;
  gesture.press(100, 20, 30);
  assert(gesture.release(500) == GestureResult::SHORT_TAP);
  gesture.press(1000, 20, 30);
  assert(gesture.hold(2999) == GestureResult::NONE);
  assert(gesture.hold(3000) == GestureResult::LONG_PRESS);
  assert(gesture.release(3100) == GestureResult::NONE);
  gesture.press(4000, 20, 30); gesture.move(30, 30);
  assert(gesture.hold(7000) == GestureResult::NONE);
  assert(gesture.release(7000) == GestureResult::NONE);
  std::puts("Full Control navigation/gesture: PASS");
}
