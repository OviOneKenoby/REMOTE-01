#include "artwork/tls_record_buffer_psram.hpp"

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
std::atomic<std::uint32_t> fallback_count{0};
std::atomic<std::size_t> fallback_bytes{0};
std::atomic<std::size_t> fallback_internal_free{0};
std::atomic<std::size_t> fallback_internal_largest{0};
std::atomic<TaskHandle_t> artwork_https_task{nullptr};

void increment_saturating(std::atomic<std::uint32_t> &counter) {
  auto value = counter.load(std::memory_order_relaxed);
  while (value != UINT32_MAX &&
         !counter.compare_exchange_weak(value, value + 1,
                                        std::memory_order_relaxed)) {}
}
}  // namespace

extern "C" void *__wrap_esp_mbedtls_mem_calloc(std::size_t count,
                                                std::size_t size) {
  if (artwork::tls_record_buffer::is_input_record_buffer_request(count, size)) {
    if (void *external = heap_caps_calloc(
            count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) {
      return external;
    }
  }

  // Keep all non-record mbedTLS objects on the pinned internal allocator first.
  void *result = __real_esp_mbedtls_mem_calloc(count, size);
  if (!result) {
    std::size_t bytes = 0;
    artwork::tls_record_buffer::request_size(count, size, bytes);
    const auto internal_free =
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const auto internal_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (artwork_https_task.load(std::memory_order_relaxed) ==
            xTaskGetCurrentTaskHandle() &&
        artwork::tls_record_buffer::is_scoped_fallback_request(bytes)) {
      if (void *external = heap_caps_calloc(
              count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)) {
        fallback_bytes.store(bytes, std::memory_order_relaxed);
        fallback_internal_free.store(internal_free, std::memory_order_relaxed);
        fallback_internal_largest.store(internal_largest,
                                        std::memory_order_relaxed);
        increment_saturating(fallback_count);
        return external;
      }
    }
    failure_bytes.store(bytes, std::memory_order_relaxed);
    failure_internal_free.store(internal_free, std::memory_order_relaxed);
    failure_internal_largest.store(internal_largest, std::memory_order_relaxed);
    increment_saturating(failure_count);
  }
  return result;
}

void artwork::tls_record_buffer::begin_artwork_https_scope() {
  artwork_https_task.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
}

void artwork::tls_record_buffer::end_artwork_https_scope() {
  auto current = xTaskGetCurrentTaskHandle();
  artwork_https_task.compare_exchange_strong(current, nullptr,
                                             std::memory_order_acq_rel);
}

artwork::tls_record_buffer::FailureSnapshot
artwork::tls_record_buffer::failure_snapshot() {
  return {failure_count.load(std::memory_order_relaxed),
          failure_bytes.load(std::memory_order_relaxed),
          failure_internal_free.load(std::memory_order_relaxed),
          failure_internal_largest.load(std::memory_order_relaxed)};
}

artwork::tls_record_buffer::FallbackSnapshot
artwork::tls_record_buffer::fallback_snapshot() {
  return {fallback_count.load(std::memory_order_relaxed),
          fallback_bytes.load(std::memory_order_relaxed),
          fallback_internal_free.load(std::memory_order_relaxed),
          fallback_internal_largest.load(std::memory_order_relaxed)};
}
