#include "artwork/online.hpp"

#include <cctype>
#include <cstring>

namespace artwork::online {
namespace {
bool object_extent(std::string_view value, std::size_t begin, std::size_t &end) {
  if (begin >= value.size() || value[begin] != '{') return false;
  unsigned depth = 0;
  bool quoted = false, escaped = false;
  for (std::size_t i = begin; i < value.size(); ++i) {
    const char c = value[i];
    if (quoted) {
      if (escaped) escaped = false;
      else if (c == '\\') escaped = true;
      else if (c == '"') quoted = false;
    } else if (c == '"') quoted = true;
    else if (c == '{') ++depth;
    else if (c == '}' && --depth == 0) { end = i + 1; return true; }
  }
  return false;
}

bool string_after(std::string_view value, std::string_view key, char *out,
                  std::size_t capacity) {
  const auto found = value.find(key);
  if (found == value.npos) return false;
  auto at = value.find(':', found + key.size());
  if (at == value.npos || (at = value.find('"', at + 1)) == value.npos)
    return false;
  ++at;
  std::size_t used = 0;
  for (; at < value.size(); ++at) {
    char c = value[at];
    if (c == '"') { out[used] = 0; return used != 0; }
    if (c == '\\') {
      if (++at >= value.size()) return false;
      c = value[at];
      if (c == 'n' || c == 'r' || c == 't') c = ' ';
      else if (c == 'u') return false;
      else if (c != '"' && c != '\\' && c != '/') return false;
    }
    if (static_cast<unsigned char>(c) < 0x20 || used + 1 >= capacity)
      return false;
    out[used++] = c;
  }
  return false;
}

template <std::size_t N>
bool normalize(std::string_view input, std::array<char, N> &out) {
  std::size_t used = 0;
  bool pending_space = false;
  for (const unsigned char c : input) {
    if (std::isspace(c)) { pending_space = used != 0; continue; }
    if (pending_space) {
      if (used + 1 >= N) return false;
      out[used++] = ' ';
      pending_space = false;
    }
    if (used + 1 >= N) return false;
    out[used++] = static_cast<char>(c < 0x80 ? std::tolower(c) : c);
  }
  out[used] = 0;
  return used != 0;
}

bool valid_uuid(std::string_view value) {
  if (value.size() != 36) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (value[i] != '-') return false;
    } else if (!std::isxdigit(static_cast<unsigned char>(value[i]))) return false;
  }
  return true;
}
} // namespace

bool station_favicon_allowed(std::string_view value) {
  if (!value.starts_with("https://") || value.size() >= favicon_capacity ||
      value.find('#') != value.npos)
    return false;
  const auto authority_end = value.find('/', 8);
  const auto authority = value.substr(8, authority_end == value.npos
                                             ? value.npos
                                             : authority_end - 8);
  if (authority.empty() || authority.find('@') != authority.npos) return false;
  for (const unsigned char c : value)
    if (c <= 0x20 || c == 0x7f) return false;
  return true;
}

bool select_station_logo(std::string_view json, const Identity &wanted,
                         StationLogoCandidate &selected) {
  selected = {};
  if (!wanted.has_station()) return false;
  std::size_t exact = 0, at = 0;
  while ((at = json.find('{', at)) != json.npos) {
    std::size_t end = 0;
    if (!object_extent(json, at, end)) return false;
    const auto object = json.substr(at, end - at);
    StationLogoCandidate candidate{};
    const bool complete =
        string_after(object, "\"stationuuid\"", candidate.station_uuid.data(),
                     candidate.station_uuid.size()) &&
        string_after(object, "\"name\"", candidate.name.data(),
                     candidate.name.size()) &&
        string_after(object, "\"favicon\"", candidate.favicon.data(),
                     candidate.favicon.size());
    std::array<char, station_capacity> normalized{};
    if (complete && normalize(std::string_view(candidate.name.data()), normalized) &&
        normalized == wanted.normalized_station) {
      ++exact;
      if (exact == 1) selected = candidate;
    }
    at = end;
  }
  if (exact != 1 || !valid_uuid(selected.station_uuid.data()) ||
      !station_favicon_allowed(selected.favicon.data())) {
    selected = {};
    return false;
  }
  return true;
}
} // namespace artwork::online
