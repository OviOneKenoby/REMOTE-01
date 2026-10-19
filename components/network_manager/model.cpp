#include "network_manager/api.hpp"
#include <algorithm>
#include <cstdio>
namespace network_manager {
namespace {
unsigned hash(const unsigned char *p, unsigned n) { unsigned h = 2166136261u; while (n--) h = (h ^ *p++) * 16777619u; return h; }
int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
bool unescape(std::string_view in, char *out, unsigned cap) {
  unsigned n = 0;
  for (unsigned i = 0; i < in.size(); ++i) {
    unsigned char c = in[i];
    if (c == '%') { if (i + 2 >= in.size() || hex(in[i + 1]) < 0 || hex(in[i + 2]) < 0) return false;
      c = (hex(in[i + 1]) << 4) | hex(in[i + 2]); i += 2;
    } else if (c == '+') c = ' ';
    if (!c || c < 32 || c == 127 || n + 1 >= cap) return false;
    out[n++] = char(c);
  }
  out[n] = 0; return true;
}
}
bool valid(const Credentials &c) {
  auto s = std::char_traits<char>::length; // only after bounded terminator checks
  if (!std::memchr(c.ssid, 0, sizeof c.ssid) || !std::memchr(c.password, 0, sizeof c.password)) return false;
  const auto sn = s(c.ssid), pn = s(c.password);
  if (!sn || sn > 32 || pn < 8 || pn > 64 || (c.security != 2 && c.security != 3) || (c.security == 3 && pn == 64)) return false;
  for (unsigned i = 0; i < sn; ++i) if (static_cast<unsigned char>(c.ssid[i]) < 32 || c.ssid[i] == 127) return false;
  for (unsigned i = 0; i < pn; ++i) if (c.password[i] < 32 || c.password[i] > 126 || (pn == 64 && hex(c.password[i]) < 0)) return false;
  return true;
}
Blob encode(const Credentials &c) {
  Blob b{}; if (!valid(c)) return b;
  b[0] = 'R'; b[1] = 'W'; b[2] = '0'; b[3] = '1'; b[4] = 1;
  b[5] = std::strlen(c.ssid); b[6] = std::strlen(c.password); b[7] = c.security;
  std::memcpy(b.data() + 8, c.ssid, b[5]); std::memcpy(b.data() + 40, c.password, b[6]);
  auto h = hash(b.data(), 104); for (unsigned i = 0; i < 4; ++i) b[104 + i] = h >> (8 * i);
  return b;
}
bool decode(const Blob &b, Credentials &c) {
  wipe(&c, sizeof c);
  if (b[0] != 'R' || b[1] != 'W' || b[2] != '0' || b[3] != '1' || b[4] != 1 || !b[5] || b[5] > 32 || b[6] < 8 || b[6] > 64) return false;
  unsigned h = 0; for (unsigned i = 0; i < 4; ++i) h |= unsigned(b[104 + i]) << (8 * i);
  if (h != hash(b.data(), 104)) return false;
  std::memcpy(c.ssid, b.data() + 8, b[5]); std::memcpy(c.password, b.data() + 40, b[6]); c.security = b[7];
  if (!valid(c) || std::strlen(c.ssid) != b[5] || std::strlen(c.password) != b[6]) { wipe(&c, sizeof c); return false; }
  return true;
}
bool parse(std::string_view body, Credentials &out) {
  Credentials c{}; unsigned fields = 0; bool ok = body.size() <= 512;
  while (ok && !body.empty()) {
    auto end = body.find('&'); auto pair = body.substr(0, end); auto sep = pair.find('=');
    if (sep == pair.npos) { ok = false; break; }
    auto key = pair.substr(0, sep), value = pair.substr(sep + 1); unsigned bit = 0;
    if (key == "ssid") { bit = 1; ok = unescape(value, c.ssid, sizeof c.ssid); }
    else if (key == "password") { bit = 2; ok = unescape(value, c.password, sizeof c.password); }
    else if (key == "security") { bit = 4; ok = value == "2" || value == "3"; c.security = value == "3" ? 3 : 2; }
    else ok = false;
    if (fields & bit) ok = false;
    fields |= bit;
    if (end == body.npos) break;
    body.remove_prefix(end + 1); if (body.empty()) ok = false;
  }
  ok = ok && fields == 7 && valid(c);
  wipe(&out, sizeof out); if (ok) out = c; wipe(&c, sizeof c); return ok;
}
const char *name(State s) {
  switch (s) { case State::PROVISIONING: return "PROVISIONING"; case State::CONNECTING: return "CONNECTING";
    case State::CONNECTED: return "CONNECTED"; case State::RETRY_WAIT: return "RETRY_WAIT"; default: return "DISABLED"; }
}
void Policy::boot(bool credentials) { ++generation; stored = credentials; candidate = false; portal = !stored; retry = 0; state = stored ? State::CONNECTING : State::PROVISIONING; deadline = 0; }
bool Policy::submit(std::uint64_t now) { if (!portal || candidate || state != State::PROVISIONING) return false; ++generation; candidate = true; state = State::CONNECTING; deadline = now + 30000; return true; }
void Policy::connected() { if (state != State::CONNECTING && state != State::RETRY_WAIT) return; stored = true; candidate = false; state = State::CONNECTED; retry = 0; deadline = 0; }
void Policy::failed(std::uint64_t now) {
  if (state == State::DISABLED || state == State::PROVISIONING) return;
  if (candidate) { candidate = false; state = State::PROVISIONING; deadline = 0; }
  else { state = State::RETRY_WAIT; deadline = now + std::min(60000u, 5000u << retry); retry = std::min(retry + 1, 4u); }
}
bool Policy::due(std::uint64_t now) { if (state != State::RETRY_WAIT || now < deadline) return false; state = State::CONNECTING; ++reconnects; return true; }
void Policy::forget() { boot(false); }
void Policy::open_setup() { ++generation; portal = true; candidate = false; retry = 0; deadline = 0; state = State::PROVISIONING; }
void Policy::stop() { ++generation; state = State::DISABLED; portal = false; candidate = false; }
void diagnostic(const Status &s, char *out, std::size_t size) {
  std::snprintf(out, size, "state=%s provisioning=%u connected=%u stored=%u storage_ready=%u rssi=%d ip=%s reconnects=%u reason=%u scanning=%u scan_count=%u rejected=%u errors=%u allocation_errors=%u stack_min_bytes=%u http_stack_min_bytes=%u",
    name(s.state), s.portal, s.connected, s.stored, s.storage_ready, s.rssi, s.ip,
    s.reconnects, s.reason, s.scanning, s.scan_count, s.rejected, s.errors, s.allocation_errors, s.stack_min, s.http_stack_min);
}
}
