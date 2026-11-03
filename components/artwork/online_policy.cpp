#include "artwork/online.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>

namespace artwork::online {
namespace {
template <std::size_t N>
bool copy(std::array<char, N> &out, std::string_view value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
    value.remove_prefix(1);
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
    value.remove_suffix(1);
  if (value.empty() || value.size() >= N) return false;
  std::copy(value.begin(), value.end(), out.begin());
  out[value.size()] = 0;
  return true;
}

template <std::size_t N, std::size_t M>
bool normalize(const std::array<char, N> &in, std::array<char, M> &out) {
  std::size_t used = 0;
  bool space = true;
  for (const unsigned char c : in) {
    if (!c) break;
    if (std::isspace(c)) { space = used != 0; continue; }
    if (space && used && used + 1 < M) out[used++] = ' ';
    space = false;
    if (used + 1 >= M) return false;
    out[used++] = static_cast<char>(c < 0x80 ? std::tolower(c) : c);
  }
  while (used && out[used - 1] == ' ') --used;
  out[used] = 0;
  return used != 0;
}

bool undesirable(std::string_view text) {
  std::array<char, 320> lower{};
  if (text.size() >= lower.size()) return true;
  std::transform(text.begin(), text.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(c < 0x80 ? std::tolower(c) : c);
  });
  const std::string_view value(lower.data(), text.size());
  constexpr std::string_view bad[] = {
      "advertisement", "commercial break", "station ident", "radio slogan",
      "listen live", "now playing", "http://", "https://", "www."};
  for (auto token : bad) if (value.find(token) != std::string_view::npos) return true;
  return false;
}

bool split_radio(std::string_view combined, std::string_view &artist,
                 std::string_view &title) {
  if (combined.empty() || undesirable(combined)) return false;
  constexpr std::string_view separators[] = {" - ", " \xe2\x80\x93 ", " \xe2\x80\x94 "};
  std::size_t found = std::string_view::npos, length = 0;
  for (auto separator : separators) {
    auto pos = combined.find(separator);
    if (pos == std::string_view::npos) continue;
    if (found != std::string_view::npos ||
        combined.find(separator, pos + separator.size()) != std::string_view::npos)
      return false;
    found = pos; length = separator.size();
  }
  if (found == std::string_view::npos) return false;
  artist = combined.substr(0, found);
  title = combined.substr(found + length);
  return !artist.empty() && !title.empty();
}
} // namespace

bool make_identity(const Metadata &metadata, std::uint64_t generation,
                   Identity &out) {
  out = {};
  if (!metadata.connected || metadata.source == Source::neutral ||
      !metadata.session || !metadata.media_generation ||
      !metadata.metadata_revision)
    return false;
  std::string_view artist, title;
  if (metadata.source == Source::bluetooth) {
    if (!metadata.artist || !metadata.title) return false;
    artist = metadata.artist; title = metadata.title;
  } else {
    if (!metadata.combined || !split_radio(metadata.combined, artist, title))
      return false;
  }
  if (undesirable(artist) || undesirable(title) ||
      !copy(out.artist, artist) || !copy(out.title, title) ||
      !normalize(out.artist, out.normalized_artist) ||
      !normalize(out.title, out.normalized_title)) return false;
  out.source = metadata.source;
  out.session = metadata.session;
  out.source_generation = metadata.source_generation;
  out.media_generation = metadata.media_generation;
  out.metadata_revision = metadata.metadata_revision;
  out.request_generation = generation;
  if (metadata.station) (void)copy(out.station, metadata.station);
  return true;
}

bool same_media(const Identity &left, const Identity &right) {
  return left.source == right.source && left.session == right.session &&
         left.source_generation == right.source_generation &&
         left.media_generation == right.media_generation &&
         left.metadata_revision == right.metadata_revision &&
         left.station == right.station && left.artist == right.artist &&
         left.title == right.title &&
         left.normalized_artist == right.normalized_artist &&
         left.normalized_title == right.normalized_title;
}

bool select_release_group(const Identity &wanted,
                          std::span<const Candidate> candidates,
                          std::array<char, mbid_capacity> &out) {
  out = {};
  int best_score = 89;
  for (const auto &candidate : candidates) {
    if (candidate.score < 90 || !candidate.release_group[0]) continue;
    std::array<char, artist_capacity> artist{};
    std::array<char, title_capacity> title{};
    if (!normalize(candidate.artist, artist) || !normalize(candidate.title, title) ||
        std::strcmp(artist.data(), wanted.normalized_artist.data()) != 0 ||
        std::strcmp(title.data(), wanted.normalized_title.data()) != 0) continue;
    if (candidate.score > best_score) {
      out = candidate.release_group;
      best_score = candidate.score;
    }
  }
  return best_score >= 90;
}

ImageFormat inspect_image(std::span<const std::uint8_t> bytes, JpegInfo &info) {
  info = {};
  if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8 ||
      bytes[bytes.size()-2] != 0xff || bytes.back() != 0xd9)
    return ImageFormat::unsupported;
  std::size_t at = 2;
  bool scan = false;
  while (at + 4 <= bytes.size()) {
    if (bytes[at++] != 0xff) return ImageFormat::unsupported;
    while (at < bytes.size() && bytes[at] == 0xff) ++at;
    if (at >= bytes.size()) return ImageFormat::unsupported;
    const auto marker = bytes[at++];
    if (marker == 0xd9) break;
    if (marker == 0xda) { scan = true; break; }
    if (marker >= 0xd0 && marker <= 0xd7) continue;
    if (at + 2 > bytes.size()) return ImageFormat::unsupported;
    const auto length = std::size_t(bytes[at] << 8 | bytes[at+1]);
    if (length < 2 || at + length > bytes.size()) return ImageFormat::unsupported;
    if (marker == 0xc2) return ImageFormat::unsupported;
    if (marker == 0xc0) {
      if (length < 8) return ImageFormat::unsupported;
      info.height = std::uint16_t(bytes[at+3] << 8 | bytes[at+4]);
      info.width = std::uint16_t(bytes[at+5] << 8 | bytes[at+6]);
      if (!info.width || !info.height || info.width > 2048 || info.height > 2048)
        return ImageFormat::unsupported;
    }
    at += length;
  }
  return info.width && info.height && scan ? ImageFormat::baseline_jpeg
                                   : ImageFormat::unsupported;
}

bool redirect_allowed(int status, std::string_view location,
                      unsigned redirects) {
  const bool redirect = status == 301 || status == 302 || status == 303 ||
                        status == 307 || status == 308;
  return redirect && redirects < 3 && location.starts_with("https://") &&
         location.size() < 384;
}

bool payload_allowed(int status, std::string_view mime, std::size_t received,
                     std::size_t limit, bool complete) {
  return status == 200 && complete && received != 0 && received <= limit &&
         (mime.starts_with("application/json") || mime.starts_with("image/jpeg"));
}

bool NegativeCache::contains(const Identity &identity, std::uint64_t now) const {
  for (const auto &entry : entries)
    if (entry.expires > now && same_media(entry.identity, identity)) return true;
  return false;
}

void NegativeCache::remember(const Identity &identity, std::uint64_t now) {
  auto *slot = &entries.front();
  for (auto &entry : entries) {
    if (same_media(entry.identity, identity) || !entry.expires || entry.expires <= now) {
      slot = &entry; break;
    }
    if (entry.age < slot->age) slot = &entry;
  }
  slot->identity = identity;
  slot->expires = now + negative_ttl_ms;
  slot->age = ++age;
}

} // namespace artwork::online
