from pathlib import Path

root = Path(__file__).resolve().parents[2]
platformio = (root / "platformio.ini").read_text(encoding="utf-8")
source = (root / "components/artwork/tls_record_buffer_psram.cpp").read_text(encoding="utf-8")
header = (root / "components/artwork/include/artwork/tls_record_buffer_psram.hpp").read_text(encoding="utf-8")
runtime = (root / "components/artwork/online_runtime.cpp").read_text(encoding="utf-8")

checks = 0


def check(value, message):
    global checks
    checks += 1
    assert value, message


check(platformio.count("-Wl,--wrap=esp_mbedtls_mem_calloc") == 1,
      "single linker wrapper")
check("CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN == 16384" in source,
      "pinned input content size guarded")
check("CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN == 4096" in source,
      "pinned output content size guarded")
check("resolved_input_buffer_bytes = 16717" in header,
      "observed input record-buffer size exact")
check("resolved_output_buffer_bytes = 4429" in header,
      "observed output record-buffer size exact")
check("MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT" in source,
      "input record buffer requests byte-addressable PSRAM")
check(source.index("heap_caps_calloc") <
      source.index("__real_esp_mbedtls_mem_calloc(count, size)"),
      "PSRAM precedes original fallback")
check("is_input_record_buffer_request(count, size)" in source,
      "external policy is exact-size restricted")
check("mbedtls_platform_set_calloc_free" not in source,
      "no runtime global allocator replacement")
check("setInsecure" not in source and "setCACert" not in source,
      "TLS verification policy unchanged")
check("std::numeric_limits<std::size_t>::max() / count" in header,
      "calloc product overflow guarded")
check("failure_snapshot" in source and "failure_bytes" in source,
      "bounded failed-allocation snapshot retained")
check("mbedtls_alloc_failure" in runtime,
      "failed allocation reported after request returns")
check("http_receive_buffer_bytes = 1024" in runtime and
      "config.buffer_size = http_receive_buffer_bytes" in runtime,
      "artwork HTTP receive buffer preserves bounded TLS internal headroom")
check("ArtworkHttpsScope allocation_scope" in runtime,
      "PSRAM fallback scope is limited to artwork HTTPS perform")
check("is_scoped_psram_request(bytes)" in source and
      "xTaskGetCurrentTaskHandle()" in source,
      "PSRAM preference requires bounded size and current artwork worker task")
check(source.index("is_scoped_psram_request(bytes)") <
      source.index("__real_esp_mbedtls_mem_calloc(count, size)"),
      "artwork-scoped non-DMA TLS state preserves DMA headroom")
check("mbedtls_psram_scoped" in runtime,
      "successful scoped PSRAM allocation remains observable")

print(f"PASS {checks}/{checks} TLS record-buffer source-policy checks")
