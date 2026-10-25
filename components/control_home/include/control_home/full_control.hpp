#pragma once

#include <cstdint>

namespace control_home {

enum class TunerSource : std::uint8_t { UNKNOWN = 0, RADIO = 1, BLUETOOTH = 2 };
enum class SourceTransition : std::uint8_t {
  STABLE = 0,
  STARTING_BLUETOOTH = 1,
  SWITCHING_RADIO = 2,
  FAILED = 3
};

struct FullControlSnapshot {
  bool connected = false;
  bool supported = false;
  bool bluetooth_connected = false;
  bool command_pending = false;
  bool result_present = false;
  bool result_accepted = false;
  std::uint8_t last_error = 0;
  TunerSource source = TunerSource::UNKNOWN;
  TunerSource requested_source = TunerSource::UNKNOWN;
  SourceTransition transition = SourceTransition::STABLE;
  std::uint64_t source_generation = 0;
};

bool full_control_read(FullControlSnapshot &snapshot, std::uint64_t &generation);
bool request_source(TunerSource source);

}  // namespace control_home
