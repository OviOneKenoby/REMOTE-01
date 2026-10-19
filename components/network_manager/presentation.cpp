#include "network_manager/presentation.hpp"
namespace network_manager {
IpText ip_text(const Status &s) {
  IpText out{};
  const char *fallback = s.state == State::PROVISIONING ? "SETUP" :
    s.state == State::CONNECTING ? "CONNECTING" :
    s.state == State::RETRY_WAIT ? "OFFLINE" : "--";
  if (s.state == State::CONNECTED && s.connected) {
    fallback = "NO IP";
    const auto *end = static_cast<const char *>(std::memchr(s.ip, 0, sizeof s.ip));
    if (end) {
      const char *p = s.ip; unsigned first = 0; bool ok = true;
      for (unsigned part = 0; part < 4 && ok; ++part) {
        const char *begin = p; unsigned value = 0;
        while (p < end && *p >= '0' && *p <= '9') {
          value = value * 10 + unsigned(*p++ - '0');
          if (p - begin > 3 || value > 255) { ok = false; break; }
        }
        ok = ok && p != begin && (p - begin == 1 || *begin != '0');
        if (!part) first = value;
        if (part < 3) { ok = ok && p < end && *p == '.'; if (ok) ++p; }
        else ok = ok && p == end;
      }
      if (ok && first > 0 && first < 224 && first != 127) {
        std::memcpy(out.text, s.ip, std::size_t(end - s.ip) + 1);
        out.assigned = true; return out;
      }
    }
  }
  std::strcpy(out.text, fallback); return out;
}
}
