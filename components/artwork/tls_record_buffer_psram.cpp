#include "artwork/tls_record_buffer_psram.hpp"

#include "esp_heap_caps.h"
#include "sdkconfig.h"

static_assert(CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN == 16384,
              "pinned mbedTLS input content length changed");

extern "C" void *__real_esp_mbedtls_mem_calloc(std::size_t count,
                                                std::size_t size);

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
  return __real_esp_mbedtls_mem_calloc(count, size);
}
