#pragma once

#include "ota_service/model.hpp"

namespace ota_service {

struct UiView {
  State state = State::IDLE;
  U64 expires_ms = 0;
  char pin[7]{};
};

struct Address {
  std::uint32_t ip = 0;
  std::uint32_t mask = 0;
  unsigned generation = 0;
  bool ready = false;
};

// Called only by the NETWORK/diagnostics lifecycle owner.
void configure(Address);
void revoke();
void set_activity_callback(void (*)());

// Cross-task, bounded observations.
bool ui_view(UiView &);
bool busy();
bool reboot_due(U64 now);
unsigned errors();
unsigned stack_min();

}  // namespace ota_service
