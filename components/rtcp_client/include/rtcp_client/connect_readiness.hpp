#pragma once
#include <cerrno>

namespace rtcp_client {
enum class ConnectReadiness { pending, ready, failed };
struct ConnectCheck { ConnectReadiness state; int error; };

inline bool connect_deadline_expired(unsigned long long now,
                                     unsigned long long deadline) {
  return deadline != 0 && now >= deadline;
}

// An expired response deadline is enforced only after the caller has made its
// one already-bounded nonblocking receive/accept pass. A response accepted by
// that pass clears `awaiting_after_receive` and therefore wins at the exact
// deadline without extending it.
inline bool response_deadline_requires_close(bool deadline_expired,
                                             bool awaiting_after_receive) {
  return deadline_expired && awaiting_after_receive;
}

// Application responses retain the three-second service deadline, followed by
// one bounded 64 ms drain window. This covers one normal REMOTE poll interval
// plus scheduling jitter without retrying or replaying the request.
inline constexpr unsigned long long response_service_deadline_ms = 3000;
inline constexpr unsigned long long response_drain_grace_ms = 64;
inline constexpr unsigned long long response_close_deadline_ms =
    response_service_deadline_ms + response_drain_grace_ms;

inline ConnectCheck classify_connect_readiness(int select_result, bool writable,
                                                int select_error, int option_result,
                                                int socket_error) {
  if (select_result < 0)
    return select_error == EINTR ? ConnectCheck{ConnectReadiness::pending, 0}
                                 : ConnectCheck{ConnectReadiness::failed, select_error};
  if (select_result == 0 || !writable) return {ConnectReadiness::pending, 0};
  if (option_result < 0) return {ConnectReadiness::failed, select_error};
  if (socket_error == 0) return {ConnectReadiness::ready, 0};
  if (socket_error == EINPROGRESS || socket_error == EALREADY || socket_error == EWOULDBLOCK)
    return {ConnectReadiness::pending, 0};
  return {ConnectReadiness::failed, socket_error};
}
} // namespace rtcp_client
