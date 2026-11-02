#include "artwork/tls_record_buffer_psram.hpp"

#include "esp_heap_caps.h"
#include "sdkconfig.h"

#include <atomic>

static_assert(CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN == 16384,
              "pinned mbedTLS input content length changed");

extern "C" void *__real_esp_mbedtls_mem_calloc(std::size_t count,
                                                std::size_t size);

namespace {
std::atomic<std::uint32_t> failure_count{0};
std::atomic<std::size_t> failure_bytes{0};
std::atomic<std::size_t> failure_internal_free{0};
std::atomic<std::size_t> failure_internal_largest{0};
}  // namespace

extern "C" void *__wrap_esp_mbedtls_mem_calloc(std::size_t count,
                                                std::size_t size) {
  if (artwork::tls_record_buffer::is_input_record_buffer_request(count, size)) {
    if (void *external = heap_caps_calloc(
            count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) {
      return external;
    }
  }

  // Keep all non-record mbedTLS objects on the pinned framework allocator.
  // Also retain its internal-memory behavior as the fail-safe fallback.
  void *result = __real_esp_mbedtls_mem_calloc(count, size);
  if (!result) {
    std::size_t bytes = 0;
    artwork::tls_record_buffer::request_size(count, size, bytes);
    failure_bytes.store(bytes, std::memory_order_relaxed);
    failure_internal_free.store(
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        std::memory_order_relaxed);
    failure_internal_largest.store(
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        std::memory_order_relaxed);
    auto value = failure_count.load(std::memory_order_relaxed);
    while (value != UINT32_MAX &&
           !failure_count.compare_exchange_weak(
               value, value + 1, std::memory_order_relaxed)) {}
  }
  return result;
}

artwork::tls_record_buffer::FailureSnapshot
artwork::tls_record_buffer::failure_snapshot() {
  return {failure_count.load(std::memory_order_relaxed),
          failure_bytes.load(std::memory_order_relaxed),
          failure_internal_free.load(std::memory_order_relaxed),
          failure_internal_largest.load(std::memory_order_relaxed)};
}
