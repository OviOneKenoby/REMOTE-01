#pragma once
#include <array>
#include <span>
#include <string_view>
#include <cstdint>
namespace network_manager::portal {
using IPv4 = std::array<unsigned char, 4>;
inline constexpr IPv4 address{192,168,4,1};
bool normalize(bool ipv6, std::span<const unsigned char>, IPv4 &);
bool socket_allowed(const IPv4 &local, const IPv4 &peer, bool sta_up, const IPv4 &sta);
bool host_valid(std::string_view);
bool canonical(std::string_view);
enum class Action { DENY, PAGE, API, REDIRECT };
Action request(bool active, bool socket_ok, std::string_view host, bool get,
               std::string_view path, bool token_ok = false, bool origin_ok = false);
enum class Probe { OTHER, ANDROID, APPLE, WINDOWS };
Probe probe(std::string_view);
// One question, bounded uncompressed name; optional bounded EDNS OPT.
// All IN A names resolve locally; other types return NOERROR/no data.
std::size_t dns_reply(bool active, std::span<const unsigned char> query, std::span<unsigned char> reply);
struct Stats {
  bool http = false, dns = false;
  unsigned accepted = 0, mapped = 0, rejected = 0, requests = 0, redirects = 0,
    http_errors = 0, last_reject = 0, dns_queries = 0, dns_answers = 0,
    dns_rejected = 0, dns_errors = 0, android = 0, apple = 0, windows = 0;
};
void diagnostic(const Stats &, char *, std::size_t);
}
