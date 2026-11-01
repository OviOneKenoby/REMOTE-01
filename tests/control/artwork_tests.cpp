#include "artwork/model.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
  using artwork::Source;
  using artwork::Presentation;
  static_assert(artwork::authoritative_source(false, 1) == Source::neutral);
  static_assert(artwork::authoritative_source(true, 0) == Source::neutral);
  static_assert(artwork::authoritative_source(true, 1) == Source::radio);
  static_assert(artwork::authoritative_source(true, 2) == Source::bluetooth);
  static_assert(artwork::authoritative_source(true, 99) == Source::neutral);
  static_assert(artwork::presentation(true) == Presentation::image);
  static_assert(artwork::presentation(false) == Presentation::text_fallback);

  artwork::Model model;
  assert(model.source == Source::neutral);
  const auto initial = model.revision;
  assert(!model.observe(false, 1));             // disconnected never leaks RADIO
  assert(model.observe(true, 1));               // authenticated RADIO
  assert(model.source == Source::radio);
  assert(!model.observe(true, 1));              // ordinary refresh does no work
  assert(model.observe(true, 2));               // RADIO -> BLUETOOTH
  assert(model.source == Source::bluetooth);
  assert(model.observe(false, 2));              // disconnect -> neutral
  assert(model.source == Source::neutral);
  assert(model.observe(true, 2));               // reconnect restores authority
  assert(model.observe(true, 1));               // BLUETOOTH -> RADIO
  assert(model.revision == initial + 5);

  std::cout << "ARTWORK_MODEL: PASS assertions=20\n";
}
