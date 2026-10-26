#pragma once

#include "control_home/full_control.hpp"

#include <cstdint>

namespace control_home {

enum class CatalogConfirmationDecision : std::uint8_t {
  NONE,
  QUERY,
  CONFIRMED,
  FAILED,
  EXPIRED
};

// Tracks read-only confirmation of one catalog PLAY. It never owns or retries
// the mutation. A disconnect changes the next action to a fresh catalog query.
struct CatalogPlaybackConfirmation {
  static constexpr std::uint32_t deadline_ms = 15000;

  CatalogCollection collection = CatalogCollection::SAVED;
  std::uint16_t token = 0;
  std::uint64_t minimum_revision = 0;
  std::uint64_t previous_sequence = 0;
  std::uint32_t deadline = 0;
  bool active = false;
  bool command_accepted = false;
  bool reconnect_pending = false;
  bool query_pending = false;

  void begin(CatalogCollection selected_collection, std::uint16_t selected_token,
             std::uint64_t revision, std::uint64_t sequence,
             std::uint32_t now) {
    collection = selected_collection;
    token = selected_token;
    minimum_revision = revision;
    previous_sequence = sequence;
    deadline = now + deadline_ms;
    active = true;
    command_accepted = false;
    reconnect_pending = false;
    query_pending = false;
  }

  void clear() { *this = {}; }

  void accepted() {
    if (active) command_accepted = true;
  }

  void disconnected() {
    if (!active) return;
    reconnect_pending = true;
    query_pending = false;
  }

  CatalogConfirmationDecision connected() {
    if (!active || !reconnect_pending || query_pending)
      return CatalogConfirmationDecision::NONE;
    reconnect_pending = false;
    query_pending = true;
    return CatalogConfirmationDecision::QUERY;
  }

  void query_started() {
    if (active) query_pending = true;
  }

  CatalogConfirmationDecision observe(bool playing,
                                      const CatalogSnapshot &page) {
    if (!active || !page.connected || page.collection != collection ||
        !page.sequence || page.sequence <= previous_sequence || !page.revision ||
        page.revision < minimum_revision)
      return CatalogConfirmationDecision::NONE;
    query_pending = page.loading;
    if (page.loading) return CatalogConfirmationDecision::NONE;

    bool selected_present = false;
    bool selected_active = false;
    for (std::uint8_t i = 0; i < page.item_count; ++i) {
      if (page.items[i].token != token) continue;
      selected_present = true;
      selected_active = page.items[i].active;
      break;
    }
    if (selected_present) {
      const auto decision = playing && selected_active
                                ? CatalogConfirmationDecision::CONFIRMED
                                : CatalogConfirmationDecision::FAILED;
      clear();
      return decision;
    }
    if (std::uint32_t(page.start) + page.item_count >= page.count) {
      clear();
      return CatalogConfirmationDecision::FAILED;
    }
    return CatalogConfirmationDecision::NONE;
  }

  CatalogConfirmationDecision tick(std::uint32_t now) {
    if (!active || static_cast<std::int32_t>(now - deadline) < 0)
      return CatalogConfirmationDecision::NONE;
    clear();
    return CatalogConfirmationDecision::EXPIRED;
  }
};

}  // namespace control_home
