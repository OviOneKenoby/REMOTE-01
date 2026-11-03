#pragma once

#include "artwork/model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace artwork::online {

constexpr std::size_t artist_capacity = 97;
constexpr std::size_t title_capacity = 160;
constexpr std::size_t mbid_capacity = 37;
constexpr std::size_t encoded_limit = 128 * 1024;
constexpr std::size_t search_limit = 64 * 1024;
constexpr std::uint64_t negative_ttl_ms = 15 * 60 * 1000ULL;

struct Identity {
  Source source = Source::neutral;
  std::uint64_t session = 0, source_generation = 0, media_generation = 0,
                metadata_revision = 0, request_generation = 0;
  std::array<char, 64> station{};
  std::array<char, artist_capacity> artist{};
  std::array<char, title_capacity> title{};
  std::array<char, artist_capacity> normalized_artist{};
  std::array<char, title_capacity> normalized_title{};
  bool operator==(const Identity &) const = default;
  bool usable() const {
    return source != Source::neutral && session && artist[0] && title[0];
  }
};
bool same_media(const Identity &, const Identity &);

struct Metadata {
  bool connected = false;
  Source source = Source::neutral;
  std::uint64_t session = 0, source_generation = 0, media_generation = 0,
                metadata_revision = 0;
  const char *station = nullptr, *artist = nullptr, *title = nullptr,
             *combined = nullptr;
};

bool make_identity(const Metadata &, std::uint64_t request_generation,
                   Identity &);

struct Candidate {
  int score = 0;
  std::array<char, artist_capacity> artist{};
  std::array<char, title_capacity> title{};
  std::array<char, mbid_capacity> release_group{};
};

// Returns true only for one unambiguous exact artist/title release group.
bool select_release_group(const Identity &, std::span<const Candidate>,
                          std::array<char, mbid_capacity> &);
std::size_t rank_release_groups(
    const Identity &, std::span<const Candidate>,
    std::span<std::array<char, mbid_capacity>>);
std::size_t parse_musicbrainz(std::string_view, Candidate *, std::size_t);

enum class ImageFormat : std::uint8_t { unsupported, baseline_jpeg };
struct JpegInfo { std::uint16_t width = 0, height = 0; };
ImageFormat inspect_image(std::span<const std::uint8_t>, JpegInfo &);

struct Lifecycle {
  bool status_available = false, connected = false, provisioning = false,
       scanning = false, ota_busy = false, commands_allowed = false,
       ui_paused = false;
};
constexpr bool eligible(const Lifecycle &value) {
  return value.status_available && value.connected && !value.provisioning &&
         !value.scanning && !value.ota_busy && value.commands_allowed &&
         !value.ui_paused;
}

// network_manager::read_status() deliberately uses a zero-wait mutex.  A
// transient unavailable sample is not an authoritative network transition.
// Keep the last authoritative network fields while still requiring one real
// sample before artwork traffic is allowed.
struct NetworkEligibilityGate {
  bool known = false, connected = false, provisioning = false,
       scanning = false;

  Lifecycle observe(bool available, bool current_connected,
                    bool current_provisioning, bool current_scanning,
                    bool ota_busy, bool commands_allowed, bool ui_paused) {
    if (available) {
      known = true;
      connected = current_connected;
      provisioning = current_provisioning;
      scanning = current_scanning;
    }
    return {known, connected, provisioning, scanning, ota_busy,
            commands_allowed, ui_paused};
  }
};
bool redirect_allowed(int status, std::string_view location,
                      unsigned redirects);
bool payload_allowed(int status, std::string_view mime, std::size_t received,
                     std::size_t limit, bool complete);

struct NegativeCache {
  struct Entry { Identity identity{}; std::uint64_t expires = 0, age = 0; };
  std::array<Entry, 4> entries{};
  std::uint64_t age = 0;
  bool contains(const Identity &, std::uint64_t now) const;
  void remember(const Identity &, std::uint64_t now);
};

struct RequestGate {
  bool pending = false;
  Identity identity{};
  bool submit(const Identity &value) {
    if (!value.usable() || (pending && identity == value)) return false;
    identity = value;
    pending = true;
    return true;
  }
  bool accept(const Identity &completed, const Identity &current) {
    if (!pending || !(completed == identity) || !(completed == current))
      return false;
    pending = false;
    return true;
  }
  void cancel() { pending = false; }
};

struct Diagnostics {
  std::uint32_t submitted = 0, completed = 0, cache_hits = 0,
                negative_hits = 0, stale = 0, network_errors = 0,
                lookup_rejected = 0, image_rejected = 0,
                allocation_failures = 0;
  std::uint32_t internal_free = 0, internal_largest = 0, psram_free = 0;
};

bool start();
bool submit(const Identity &);
bool copy_completion(const Identity &, std::span<std::uint16_t> pixels);
void cancel();
Diagnostics diagnostics();

} // namespace artwork::online
