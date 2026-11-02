#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace artwork::tls_record_buffer {

struct FailureSnapshot {
  std::uint32_t count = 0;
  std::size_t bytes = 0;
  std::size_t internal_free = 0;
  std::size_t internal_largest = 0;
};

// Resolved from the pinned ESP-IDF 5.5.3 mbedTLS configuration. The target
// translation unit verifies this against MBEDTLS_SSL_IN_BUFFER_LEN.
constexpr std::size_t resolved_input_buffer_bytes = 16717;

inline bool request_size(std::size_t count, std::size_t size,
                         std::size_t &bytes) {
  if (count != 0 && size > std::numeric_limits<std::size_t>::max() / count) {
    return false;
  }
  bytes = count * size;
  return true;
}

inline bool is_input_record_buffer_request(std::size_t count,
                                           std::size_t size) {
  std::size_t bytes = 0;
  return request_size(count, size, bytes) &&
         bytes == resolved_input_buffer_bytes;
}

FailureSnapshot failure_snapshot();

}  // namespace artwork::tls_record_buffer
