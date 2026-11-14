#include "factory_reset_model.hpp"
#include "network_manager/recovery.hpp"
#include <cstdio>

namespace {
unsigned assertions = 0;
#define CHECK(x) do { ++assertions; if (!(x)) { std::printf("FAIL line=%d %s\n", __LINE__, #x); return false; } } while (0)
using namespace factory_reset;
using network_manager::Recovery;
using network_manager::SettingsAction;
using network_manager::SettingsResult;

// The numeric values must mirror network_manager::SettingsResult.
static_assert(unsigned(Result::NONE) == unsigned(SettingsResult::NONE));
static_assert(unsigned(Result::ACCEPTED) == unsigned(SettingsResult::ACCEPTED));
static_assert(unsigned(Result::ARMED) == unsigned(SettingsResult::ARMED));
static_assert(unsigned(Result::REFUSED) == unsigned(SettingsResult::REFUSED));
static_assert(unsigned(Result::STORAGE_ERROR) == unsigned(SettingsResult::STORAGE_ERROR));
static_assert(unsigned(Result::SERVICE_ERROR) == unsigned(SettingsResult::SERVICE_ERROR));

// Stand-in for the NETWORK owner: the real Recovery authorizer, a counted erase.
struct Owner {
  Recovery rec;
  unsigned generation = 5;
  bool storage_ok = true, responsive = true;
  int erase_calls = 0, restarts = 0, arm_requests = 0, confirm_requests = 0;
  std::uint64_t now_ms = 1000;
  View view() const { return {generation, rec.ticket, static_cast<Result>(rec.result)}; }
  void request(SettingsAction a, std::uint64_t ticket) {
    if (!responsive) return;
    if (a == SettingsAction::ARM_WIFI_RESET) ++arm_requests; else ++confirm_requests;
    if (rec.authorize(a, generation, generation, ticket, now_ms) && a == SettingsAction::CONFIRM_WIFI_RESET) {
      if (storage_ok) ++erase_calls; else rec.result = SettingsResult::STORAGE_ERROR;
    }
  }
};

Inputs inputs(const Owner &o, bool blocked = false, bool allowed = true) {
  Inputs in; in.blocked = blocked; in.commands_allowed = allowed; in.view = o.view(); return in;
}
// One 50 ms step: tick, then execute the requested action like user_app does.
Action step(Machine &m, Owner &o, std::uint32_t &t, bool blocked = false, bool allowed = true) {
  t += 50; o.now_ms += 50;
  const auto a = m.tick(t, inputs(o, blocked, allowed));
  if (a == Action::REQUEST_ARM) o.request(SettingsAction::ARM_WIFI_RESET, 0);
  if (a == Action::REQUEST_CONFIRM) o.request(SettingsAction::CONFIRM_WIFI_RESET, m.ticket);
  if (a == Action::REQUEST_RESTART) ++o.restarts;
  return a;
}
void run(Machine &m, Owner &o, std::uint32_t &t, std::uint32_t ms) { for (std::uint32_t i = 0; i < ms; i += 50) step(m, o, t); }

bool short_tap_does_nothing() {
  Machine m; Owner o; std::uint32_t t = 0;
  m.press(t, inputs(o)); CHECK(m.state == State::HOLDING);
  run(m, o, t, 1900); CHECK(m.state == State::HOLDING && !m.modal_open());
  CHECK(m.progress_permille(t) > 900 && m.progress_permille(t) < 1000);
  m.release(); CHECK(m.state == State::IDLE);
  run(m, o, t, 5000);
  CHECK(m.state == State::IDLE && o.erase_calls == 0 && o.arm_requests == 0 && o.restarts == 0);
  return true;
}
bool hold_time_boundary() {
  Machine m; Owner o; std::uint32_t t = 0;
  m.press(0, inputs(o));
  CHECK(m.tick(1999, inputs(o)) == Action::NONE && m.state == State::HOLDING && m.progress_permille(1999) == 999);
  CHECK(m.tick(2000, inputs(o)) == Action::NONE && m.state == State::MODAL);
  (void)t;
  return true;
}
bool blocked_or_sleeping_never_starts_or_cancels() {
  Owner o; Machine a; a.press(0, inputs(o, true)); CHECK(a.state == State::IDLE);          // OTA busy
  Machine b; b.press(0, inputs(o, false, false)); CHECK(b.state == State::IDLE);           // display off
  Machine c; c.press(0, inputs(o)); c.tick(500, inputs(o, true)); CHECK(c.state == State::IDLE);   // busy during hold
  Machine d; d.press(0, inputs(o)); d.tick(500, inputs(o, false, false)); CHECK(d.state == State::IDLE);
  Machine e; e.press(0, inputs(o)); e.tick(2000, inputs(o)); CHECK(e.state == State::MODAL);
  e.tick(2100, inputs(o, false, false)); CHECK(e.state == State::IDLE);                    // screen off cancels the modal
  Machine f; f.press(0, inputs(o)); f.tick(2000, inputs(o)); f.tick(2100, inputs(o, true)); CHECK(f.state == State::IDLE);
  CHECK(o.erase_calls == 0);
  return true;
}
bool modal_cancel_timeout_outside() {
  Owner o; std::uint32_t t = 0;
  { Machine m; m.press(0, inputs(o)); m.tick(2000, inputs(o)); CHECK(m.modal_open()); m.cancel(); CHECK(m.state == State::IDLE && !m.modal_open()); }
  { Machine m; m.press(0, inputs(o)); m.tick(2000, inputs(o)); m.tick(2000 + 9999, inputs(o)); CHECK(m.state == State::MODAL);
    m.tick(2000 + 10000, inputs(o)); CHECK(m.state == State::IDLE); }
  { Machine m; m.press(0, inputs(o)); m.tick(2000, inputs(o)); m.outside_touch(); CHECK(m.state == State::IDLE); }
  // cancel/outside are no-ops outside the MODAL state
  { Machine m; m.cancel(); m.outside_touch(); CHECK(m.state == State::IDLE); }
  CHECK(o.erase_calls == 0 && o.arm_requests == 0); (void)t;
  return true;
}
bool confirm_flow_erases_once_then_restarts() {
  Machine m; Owner o; std::uint32_t t = 0;
  m.press(t, inputs(o)); run(m, o, t, 2000); CHECK(m.state == State::MODAL);
  run(m, o, t, 3000); CHECK(m.state == State::MODAL);
  CHECK(o.erase_calls == 0 && o.arm_requests == 0);       // nothing before CONFIRM
  m.confirm(t, inputs(o)); CHECK(m.state == State::ARMING);
  run(m, o, t, 100); CHECK(o.arm_requests == 1);
  run(m, o, t, 100); CHECK(o.confirm_requests == 1 && o.erase_calls == 1);
  CHECK(m.ticket != 0 && o.rec.ticket == 0 && o.rec.result == SettingsResult::ACCEPTED);
  run(m, o, t, 100); CHECK(m.state == State::RESTARTING && o.restarts == 0);   // "Restarting..." first
  run(m, o, t, restart_show_ms + 100); CHECK(o.restarts == 1);
  run(m, o, t, 2000); CHECK(o.restarts == 1 && o.erase_calls == 1 && o.arm_requests == 1 && o.confirm_requests == 1);
  return true;
}
bool storage_error_stays_and_never_restarts() {
  Machine m; Owner o; o.storage_ok = false; std::uint32_t t = 0;
  m.press(t, inputs(o)); run(m, o, t, 2000); m.confirm(t, inputs(o));
  run(m, o, t, 600);
  CHECK(m.state == State::ERROR && o.erase_calls == 0 && o.restarts == 0);
  run(m, o, t, error_show_ms + 100); CHECK(m.state == State::IDLE && o.restarts == 0);
  Machine k; Owner q; q.storage_ok = false; std::uint32_t u = 0;
  k.press(u, inputs(q)); run(k, q, u, 2000); k.confirm(u, inputs(q)); run(k, q, u, 600);
  CHECK(k.state == State::ERROR); k.dismiss_error(); CHECK(k.state == State::IDLE);
  return true;
}
bool unresponsive_owner_times_out_without_erase() {
  Machine m; Owner o; o.responsive = false; std::uint32_t t = 0;
  m.press(t, inputs(o)); run(m, o, t, 2000); m.confirm(t, inputs(o));
  run(m, o, t, arm_wait_ms + 200);
  CHECK(m.state == State::ERROR && o.erase_calls == 0 && o.restarts == 0 && o.confirm_requests == 0);
  return true;
}
bool stale_ticket_is_not_reused() {
  Machine m; Owner o; std::uint32_t t = 0;
  // An old armed ticket (an earlier, abandoned attempt) is still published.
  o.request(SettingsAction::ARM_WIFI_RESET, 0);
  const auto old_ticket = o.rec.ticket; CHECK(old_ticket != 0 && o.rec.result == SettingsResult::ARMED);
  m.press(t, inputs(o)); run(m, o, t, 2000); m.confirm(t, inputs(o));
  run(m, o, t, 100);
  CHECK(o.arm_requests == 2);                    // a fresh arm was requested
  run(m, o, t, 100);
  CHECK(m.ticket != old_ticket && o.erase_calls == 1);
  return true;
}
bool blocked_during_arming_aborts_before_erase() {
  Machine m; Owner o; std::uint32_t t = 0;
  m.press(t, inputs(o)); run(m, o, t, 2000); m.confirm(t, inputs(o));
  step(m, o, t, true);
  CHECK(m.state == State::ERROR && o.erase_calls == 0 && o.confirm_requests == 0);
  return true;
}
bool restart_refusal_shows_error() {
  Machine m; Owner o; std::uint32_t t = 0;
  m.press(t, inputs(o)); run(m, o, t, 2000); m.confirm(t, inputs(o)); run(m, o, t, 300 + restart_show_ms + 100);
  CHECK(o.restarts == 1 && m.state == State::RESTARTING);
  m.restart_failed(t); CHECK(m.state == State::ERROR);
  return true;
}
bool erase_never_without_confirm_random() {
  unsigned seed = 7;
  for (unsigned round = 0; round < 2000; ++round) {
    Machine m; Owner o; std::uint32_t t = 0;
    for (unsigned i = 0; i < 80; ++i) {
      seed = seed * 1664525u + 1013904223u;
      switch ((seed >> 16) % 6) {
      case 0: m.press(t, inputs(o, (seed >> 8) & 1)); break;
      case 1: m.release(); break;
      case 2: m.cancel(); break;
      case 3: m.outside_touch(); break;
      case 4: m.dismiss_error(); break;
      default: break;
      }
      step(m, o, t, false, ((seed >> 4) & 7) != 0);
    }
    CHECK(o.erase_calls == 0 && o.arm_requests == 0 && o.restarts == 0);   // confirm() was never called
  }
  return true;
}
} // namespace

int main() {
  struct Test { const char *name; bool (*run)(); };
  const Test tests[] = {{"short_tap_does_nothing", short_tap_does_nothing},
                        {"hold_time_boundary", hold_time_boundary},
                        {"blocked_or_sleeping_never_starts_or_cancels", blocked_or_sleeping_never_starts_or_cancels},
                        {"modal_cancel_timeout_outside", modal_cancel_timeout_outside},
                        {"confirm_flow_erases_once_then_restarts", confirm_flow_erases_once_then_restarts},
                        {"storage_error_stays_and_never_restarts", storage_error_stays_and_never_restarts},
                        {"unresponsive_owner_times_out_without_erase", unresponsive_owner_times_out_without_erase},
                        {"stale_ticket_is_not_reused", stale_ticket_is_not_reused},
                        {"blocked_during_arming_aborts_before_erase", blocked_during_arming_aborts_before_erase},
                        {"restart_refusal_shows_error", restart_refusal_shows_error},
                        {"erase_never_without_confirm_random", erase_never_without_confirm_random}};
  unsigned pass = 0, fail = 0;
  for (const auto &t : tests) {
    const bool ok = t.run();
    std::printf("%s FACTORY_RESET %s\n", ok ? "PASS" : "FAIL", t.name);
    ok ? ++pass : ++fail;
  }
  std::printf("FACTORY RESET SUMMARY PASS=%u FAIL=%u assertions=%u\n", pass, fail, assertions);
  return fail ? 1 : 0;
}
