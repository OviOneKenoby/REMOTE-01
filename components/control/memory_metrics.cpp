#include "control/core.hpp"

// Read from the target ELF: target ABI measurements, not host-size estimates.
extern "C" {
extern const unsigned remote01_control_sizes[] = {
    sizeof(control::Text), sizeof(control::Id), sizeof(control::CommandRequest),
    sizeof(control::CommandResult), sizeof(control::StateObservation),
    sizeof(control::CapabilityDescriptor), sizeof(control::Record),
    sizeof(control::Effect), sizeof(control::Store), sizeof(control::Core)};
}
