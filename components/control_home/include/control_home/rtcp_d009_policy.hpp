#pragma once
#include <cstdint>

namespace control_home::d009 {

enum class ResultClass : std::uint8_t {
  invalid_argument,
  unsupported,
  unavailable,
  stale,
  execution_failed,
  busy,
  internal,
  invalid
};

constexpr ResultClass classify_result(std::uint8_t error) {
  switch (error) {
  case 1: return ResultClass::invalid_argument;
  case 2: return ResultClass::unsupported;
  case 3: return ResultClass::unavailable;
  case 4: return ResultClass::stale;
  case 5: return ResultClass::execution_failed;
  case 6: return ResultClass::busy;
  case 7: return ResultClass::internal;
  default: return ResultClass::invalid;
  }
}

struct FenceDecision {
  bool accepted = false;
  bool new_context = false;
};

// Fixed-size session-local ordering state for the shared TUNER sample sequence.
struct MediaFence {
  std::uint64_t sequence = 0, generation = 0, metadata_revision = 0;

  void reset() { sequence = generation = metadata_revision = 0; }

  FenceDecision snapshot(std::uint64_t next_sequence,
                         std::uint64_t next_generation) {
    return accept(next_sequence, next_generation, 0, false);
  }

  FenceDecision metadata(std::uint64_t next_sequence,
                         std::uint64_t next_generation,
                         std::uint64_t next_revision) {
    return accept(next_sequence, next_generation, next_revision, true);
  }

private:
  FenceDecision accept(std::uint64_t next_sequence,
                       std::uint64_t next_generation,
                       std::uint64_t next_revision, bool has_revision) {
    if (!next_sequence || !next_generation || next_sequence <= sequence ||
        (generation && next_generation < generation) ||
        (has_revision && !next_revision) ||
        (has_revision && next_generation == generation &&
         next_revision < metadata_revision))
      return {};
    const bool changed = next_generation != generation;
    sequence = next_sequence;
    generation = next_generation;
    if (changed) metadata_revision = 0;
    if (has_revision) metadata_revision = next_revision;
    return {true, changed};
  }
};

} // namespace control_home::d009
