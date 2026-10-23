#include "control_home/rtcp_device.hpp"

namespace rtcp_client {
bool Client::initialize() { return false; }
void Client::suspend() {}
}

namespace control_home {
void RtcpDevice::dispatch(const control::Dispatch &, control::AdapterSink &) {}
bool RtcpDevice::install(control::Core &, control::ingress::Boundary &,
                         const control::Id &, const control::Id &,
                         const control::Id &, control::U64) {
  return false;
}
bool RtcpDevice::poll(control::U64, bool) { return true; }
} // namespace control_home
