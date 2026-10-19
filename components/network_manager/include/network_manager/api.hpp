#pragma once
#include "model.hpp"
#include "portal.hpp"
#include "recovery.hpp"
#include "sleep.hpp"
namespace network_manager {
struct Status {
  portal::Stats services;
  State state = State::DISABLED;
  bool portal = false, connected = false, stored = false, storage_ready = false, scanning = false;
  unsigned generation = 0, reconnects = 0, reason = 0, scan_count = 0, errors = 0, allocation_errors = 0, rejected = 0, stack_min = 0, http_stack_min = 0;
  int rssi = 0; char ip[16]{};
};
struct LocalSetup { bool active = false; unsigned show = 0; char ssid[24]{}, key[17]{}; };
struct Network { char ssid[33]{}; int rssi = 0; unsigned security = 0; };
struct WebView { Status status; char token[33]{}; Network networks[16]{}; };
void diagnostic(const Status &, char *, std::size_t); // Secret-free type and formatter shared with host tests.
void start();
unsigned sleep_prepare(); // POWER only; fixed epoch mailbox, no SDK calls.
bool sleep_ready(unsigned);
sleep::Reply sleep_reply(unsigned);
void sleep_resume(unsigned); // POWER cancel/wake, NETWORK performs restoration.
bool read_status(Status &); // Secret-free copy; zero-wait, safe for LVGL owner.
struct SettingsView { unsigned generation = 0; std::uint64_t confirmation = 0, expires_ms = 0; SettingsResult result = SettingsResult::NONE; };
bool read_settings(SettingsView &); // Zero-wait copied local confirmation/result.
bool settings_request(SettingsAction, unsigned generation, std::uint64_t confirmation = 0);
// Enqueue only; acceptance/result is owner-published. FACTORY_RESET is refused.
bool development_key(unsigned char);
bool development_command(unsigned char);
bool web_view(WebView &);
bool local_setup(LocalSetup &);
bool submit(const Credentials &, unsigned generation);
bool scan(unsigned generation);
void ui_tick(); // LVGL owner only, temporary local key card; no HOME/Core semantics
void fault(bool allocation = false);
}
