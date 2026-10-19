#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string_view>
namespace network_manager {
inline void wipe(void *p, std::size_t n) { auto *v = static_cast<volatile unsigned char *>(p); while (n--) *v++ = 0; }
struct Credentials { char ssid[33]{}; char password[65]{}; unsigned char security = 2; };
bool valid(const Credentials &);
using Blob = std::array<unsigned char, 108>;
Blob encode(const Credentials &);
bool decode(const Blob &, Credentials &);
bool parse(std::string_view, Credentials &); // strict URL-encoded SSID/password/security only
enum class State { DISABLED, PROVISIONING, CONNECTING, CONNECTED, RETRY_WAIT };
const char *name(State);
struct Policy {
  State state = State::DISABLED;
  bool stored = false, candidate = false, portal = false;
  unsigned retry = 0, reconnects = 0, generation = 0;
  std::uint64_t deadline = 0;
  void boot(bool credentials);
  bool submit(std::uint64_t now);
  void connected();
  void failed(std::uint64_t now);
  bool due(std::uint64_t now);
  void forget();
  void open_setup(); // Explicit local recovery; retain stored credential ownership.
  void stop();
};
enum class Kind { SCAN, SUBMIT, GOT_IP, DISCONNECTED, SCAN_DONE, STATUS, ARM_CLEAR, CONFIRM_CLEAR, FAULT, REBOOT, SETTINGS };
struct Message { Kind kind{}; unsigned generation = 0, detail = 0; Credentials credentials{}; std::uint64_t confirmation = 0; };
// All producers are tasks, never ISRs. Try-lock is nonblocking and overflow is explicit.
struct Inbox {
  std::atomic_flag lock = ATOMIC_FLAG_INIT;
  std::array<Message, 8> slots{};
  unsigned head = 0, count = 0;
  bool sealed = false;
  bool seal() {
    if(lock.test_and_set(std::memory_order_acquire))return false;
    bool ok=count==0;if(ok)sealed=true;lock.clear(std::memory_order_release);return ok;
  }
  bool unseal() {
    if(lock.test_and_set(std::memory_order_acquire))return false;
    sealed=false;lock.clear(std::memory_order_release);return true;
  }
  bool post(const Message &m) {
    if (lock.test_and_set(std::memory_order_acquire)) return false;
    bool ok = !sealed && count < slots.size();
    if (ok) { slots[(head + count) % slots.size()] = m; ++count; }
    lock.clear(std::memory_order_release); return ok;
  }
  bool pop(Message &m) {
    if (lock.test_and_set(std::memory_order_acquire)) return false;
    bool ok = count != 0;
    if (ok) { m = slots[head]; wipe(&slots[head], sizeof(Message)); head = (head + 1) % slots.size(); --count; }
    lock.clear(std::memory_order_release); return ok;
  }
};
template<class Storage> bool replace(Storage &s, const Credentials &c) {
  if (!valid(c)) return false;
  auto b = encode(c); bool ok = s.write("remote_wifi", "credentials", b.data(), b.size());
  wipe(b.data(), b.size()); return ok;
}
template<class Storage> bool erase(Storage &s) { return s.erase("remote_wifi", "credentials"); }
}
