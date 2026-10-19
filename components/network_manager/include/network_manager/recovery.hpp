#pragma once
#include "model.hpp"
namespace network_manager {
// Local firmware API only: no HTTP route or network authorization is implied.
enum class SettingsAction : unsigned { OPEN_SETUP, CANCEL_SETUP, ARM_WIFI_RESET, CONFIRM_WIFI_RESET, FACTORY_RESET };
enum class SettingsResult : unsigned { NONE, ACCEPTED, ARMED, REFUSED, STORAGE_ERROR, SERVICE_ERROR };
struct Recovery {
  std::uint64_t serial = 0, ticket = 0, deadline = 0;
  unsigned epoch = 0;
  SettingsResult result = SettingsResult::NONE;
  void invalidate() { ticket = deadline = 0; }
  bool authorize(SettingsAction action, unsigned requested_epoch, unsigned current_epoch,
                 std::uint64_t supplied_ticket, std::uint64_t now) {
    result = SettingsResult::REFUSED;
    if (requested_epoch != current_epoch || action == SettingsAction::FACTORY_RESET) return false;
    switch (action) {
    case SettingsAction::OPEN_SETUP: case SettingsAction::CANCEL_SETUP:
      invalidate(); result = SettingsResult::ACCEPTED; return true;
    case SettingsAction::ARM_WIFI_RESET:
      invalidate();
      if (serial == UINT64_MAX || now > UINT64_MAX - 10000) return false;
      ticket = ++serial; epoch = current_epoch; deadline = now + 10000;
      result = SettingsResult::ARMED; return false;
    case SettingsAction::CONFIRM_WIFI_RESET: {
      const bool ok = ticket && supplied_ticket == ticket && epoch == current_epoch && now <= deadline;
      invalidate(); result = ok ? SettingsResult::ACCEPTED : SettingsResult::REFUSED; return ok;
    }
    default: return false;
    }
  }
};
}
