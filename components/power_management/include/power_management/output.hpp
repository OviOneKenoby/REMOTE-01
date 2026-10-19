#pragma once

namespace power_management {
// Small hardware seam: only commit the observed software state on successful
// output write. Failed quiesce/display operations can never release power.
template<class Board> bool set_display(Board &b, bool &current, bool wanted) {
  if (current == wanted) return true;
  if (!b.display(wanted)) return false;
  current = wanted; return true;
}
template<class Board> bool controlled_release(Board &b, bool owner_parked,
                                              bool ui_parked, bool display, bool &hold) {
  if (!owner_parked || !ui_parked || display || !hold || !b.release_hold()) return false;
  hold = false; return true;
}
} // namespace power_management
