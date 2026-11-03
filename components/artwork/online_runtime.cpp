#include "artwork/online.hpp"
#include "artwork/tls_record_buffer_psram.hpp"

#include "network_manager/api.hpp"
#include "ota_service/api.hpp"
#include "power_management/api.hpp"
#include "lvgl.h"

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp32s3/rom/tjpgd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>
#include <strings.h>
#include <string_view>

namespace artwork::online {
namespace {
constexpr char tag[] = "ARTWORK";
constexpr std::size_t pixels = 144 * 144;
constexpr std::size_t jpeg_pool_size = 8192;
constexpr unsigned http_timeout_ms = 8000;
// ESP HTTP buffers are allocated from internal RAM by the pinned framework.
// Artwork responses are streamed into the bounded PSRAM sink, so 1 KiB is
// sufficient here and preserves a contiguous block for TLS certificate MPI.
constexpr int http_receive_buffer_bytes = 1024;
constexpr unsigned worker_stack_bytes = 16 * 1024;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t worker_handle = nullptr;
struct RuntimeState {
  Identity requested{}, completed{}, positive_identity{};
  NegativeCache negatives{};
};
RuntimeState *runtime = nullptr;
bool request_ready = false;
std::atomic_bool completion_ready{false};
std::uint8_t *encoded = nullptr, *jpeg_pool = nullptr;
std::uint16_t *decoded = nullptr;
Diagnostics stats{};
NetworkEligibilityGate network_gate{};
std::int64_t last_musicbrainz_us = 0;

std::uint64_t now_ms() { return std::uint64_t(esp_timer_get_time() / 1000); }
void increment(std::uint32_t &value) { if (value != UINT32_MAX) ++value; }
void record(std::uint32_t &value) {
  portENTER_CRITICAL(&mux); increment(value); portEXIT_CRITICAL(&mux);
}

bool allowed() {
  network_manager::Status network{};
  const bool available = network_manager::read_status(network);
  return eligible(network_gate.observe(
      available, network.connected, network.portal, network.scanning,
      ota_service::busy(), power_management::commands_allowed(),
      power_management::ui_pause_requested()));
}

struct HttpSink {
  std::uint8_t *data = nullptr;
  std::size_t capacity = 0, size = 0;
  bool overflow = false;
  char content_type[48]{}, location[384]{};
};

esp_err_t http_event(esp_http_client_event_t *event) {
  auto &sink = *static_cast<HttpSink *>(event->user_data);
  if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
    const auto count = std::size_t(event->data_len);
    if (count > sink.capacity - sink.size) sink.overflow = true;
    else {
      std::memcpy(sink.data + sink.size, event->data, count);
      sink.size += count;
    }
  } else if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key &&
             event->header_value) {
    auto copy_header = [&](char *out, std::size_t capacity) {
      std::snprintf(out, capacity, "%s", event->header_value);
    };
    if (strcasecmp(event->header_key, "Content-Type") == 0)
      copy_header(sink.content_type, sizeof sink.content_type);
    else if (strcasecmp(event->header_key, "Location") == 0)
      copy_header(sink.location, sizeof sink.location);
  }
  return ESP_OK;
}

struct HttpResult { int status = 0; std::size_t size = 0; char type[48]{}; };

bool https_get(const char *initial_url, std::size_t limit, HttpResult &result) {
  char url[512]{};
  if (!initial_url || std::strlen(initial_url) >= sizeof url) return false;
  std::strcpy(url, initial_url);
  for (unsigned redirect = 0; redirect <= 3; ++redirect) {
    if (std::strncmp(url, "https://", 8) != 0) {
      ESP_LOGW(tag, "https_rejected reason=scheme redirect=%u", redirect);
      return false;
    }
    if (!allowed()) {
      ESP_LOGW(tag, "https_rejected reason=lifecycle redirect=%u", redirect);
      return false;
    }
    HttpSink sink{encoded, limit};
    char user_agent[128]{};
    const auto *description = esp_app_get_description();
    std::snprintf(user_agent, sizeof user_agent,
                  "REMOTE01/%s (https://github.com/OviOneKenoby/REMOTE-01)",
                  description ? description->version : "unknown");
    esp_http_client_config_t config{};
    config.url = url;
    config.user_agent = user_agent;
    config.timeout_ms = http_timeout_ms;
    config.event_handler = http_event;
    config.user_data = &sink;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true;
    config.buffer_size = http_receive_buffer_bytes;
    config.buffer_size_tx = 1024;
    auto client = esp_http_client_init(&config);
    if (!client) {
      ESP_LOGW(tag, "https_rejected reason=client_init redirect=%u", redirect);
      return false;
    }
    const auto failures_before = tls_record_buffer::failure_snapshot();
    const auto fallbacks_before = tls_record_buffer::fallback_snapshot();
    esp_err_t error = ESP_FAIL;
    {
      tls_record_buffer::ArtworkHttpsScope allocation_scope;
      error = esp_http_client_perform(client);
    }
    const auto failures_after = tls_record_buffer::failure_snapshot();
    const auto fallbacks_after = tls_record_buffer::fallback_snapshot();
    const int status = error == ESP_OK ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);
    if (failures_after.count != failures_before.count) {
      ESP_LOGW(tag,
               "mbedtls_alloc_failure count=%u bytes=%u INT_free=%u INT_largest=%u",
               unsigned(failures_after.count), unsigned(failures_after.bytes),
               unsigned(failures_after.internal_free),
               unsigned(failures_after.internal_largest));
    }
    if (fallbacks_after.count != fallbacks_before.count) {
      ESP_LOGI(tag,
               "mbedtls_psram_scoped count=%u bytes=%u INT_free=%u INT_largest=%u",
               unsigned(fallbacks_after.count), unsigned(fallbacks_after.bytes),
               unsigned(fallbacks_after.internal_free),
               unsigned(fallbacks_after.internal_largest));
    }
    if (error != ESP_OK || sink.overflow) {
      ESP_LOGW(tag,
               "https_rejected reason=%s redirect=%u error=%d status=%d bytes=%u",
               sink.overflow ? "overflow" : "transport", redirect, int(error),
               status, unsigned(sink.size));
      return false;
    }
    if (status == 301 || status == 302 || status == 303 || status == 307 ||
        status == 308) {
      if (!redirect_allowed(status, sink.location, redirect) ||
          std::strlen(sink.location) >= sizeof url) {
        ESP_LOGW(tag,
                 "https_rejected reason=redirect status=%d redirect=%u location_bytes=%u",
                 status, redirect, unsigned(std::strlen(sink.location)));
        return false;
      }
      std::strcpy(url, sink.location);
      continue;
    }
    result.status = status;
    result.size = sink.size;
    std::snprintf(result.type, sizeof result.type, "%s", sink.content_type);
    const bool accepted =
        payload_allowed(status, sink.content_type, sink.size, limit, true);
    if (!accepted)
      ESP_LOGW(tag,
               "https_rejected reason=payload redirect=%u status=%d bytes=%u type=%s",
               redirect, status, unsigned(sink.size), sink.content_type);
    return accepted;
  }
  return false;
}

bool append_encoded(char *out, std::size_t capacity, std::size_t &used,
                    std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  for (unsigned char c : value) {
    const bool safe = std::isalnum(c) || c == '-' || c == '_' || c == '.';
    const std::size_t count = safe ? 1 : 3;
    if (used + count >= capacity) return false;
    if (safe) out[used++] = static_cast<char>(c);
    else { out[used++] = '%'; out[used++] = hex[c >> 4]; out[used++] = hex[c & 15]; }
  }
  out[used] = 0;
  return true;
}

bool search_url(const Identity &identity, char *out, std::size_t capacity) {
  // Search release groups directly.  A recording search can rank a compilation
  // release ahead of the canonical single even when artist/title are exact.
  const char prefix[] = "https://musicbrainz.org/ws/2/release-group/?fmt=json&limit=5&query=releasegroup%3A%22";
  std::size_t used = sizeof prefix - 1;
  if (used >= capacity) return false;
  std::memcpy(out, prefix, used + 1);
  constexpr char middle[] = "%22%20AND%20artist%3A%22";
  if (!append_encoded(out, capacity, used, identity.title.data()) ||
      used + sizeof middle >= capacity) return false;
  std::memcpy(out + used, middle, sizeof middle - 1);
  used += sizeof middle - 1; out[used] = 0;
  if (!append_encoded(out, capacity, used, identity.artist.data()) ||
      used + 4 >= capacity) return false;
  std::memcpy(out + used, "%22", 4); used += 3;
  return true;
}

struct JpegInput { const std::uint8_t *data; std::size_t size, offset; };
struct JpegOutput { std::uint16_t *pixels; int crop_x, crop_y; };

bool decode_jpeg(std::size_t size) {
  JpegInfo info{};
  if (inspect_image({encoded, size}, info) != ImageFormat::baseline_jpeg) return false;
  JDEC decoder{};
  struct Context { JpegInput input; JpegOutput output; } context{{encoded,size,0},{decoded,0,0}};
  auto input_cb = [](JDEC *j, BYTE *buffer, UINT count) -> UINT {
    auto &c = *static_cast<Context *>(j->device);
    const auto available = std::min<std::size_t>(count, c.input.size - c.input.offset);
    if (buffer) std::memcpy(buffer, c.input.data + c.input.offset, available);
    c.input.offset += available; return UINT(available);
  };
  auto output_cb = [](JDEC *j, void *bitmap, JRECT *rect) -> UINT {
    auto &c = *static_cast<Context *>(j->device);
    const auto *rgb = static_cast<const std::uint8_t *>(bitmap);
    const int bw = rect->right - rect->left + 1;
    for (int y=rect->top;y<=rect->bottom;++y) for(int x=rect->left;x<=rect->right;++x){
      const int i=((y-rect->top)*bw+(x-rect->left))*3,dx=x-c.output.crop_x,dy=y-c.output.crop_y;
      if(dx>=0&&dx<144&&dy>=0&&dy<144){const auto r=rgb[i],g=rgb[i+1],b=rgb[i+2];
        c.output.pixels[dy*144+dx]=lv_color_make(r,g,b).full;}
    } return 1;
  };
  if (jd_prepare(&decoder, input_cb, jpeg_pool, jpeg_pool_size, &context) != JDR_OK)
    return false;
  unsigned scale = 0;
  while (scale < 3 && (decoder.width >> (scale + 1)) >= 144 &&
         (decoder.height >> (scale + 1)) >= 144) ++scale;
  const int width = decoder.width >> scale, height = decoder.height >> scale;
  if (width < 144 || height < 144) return false;
  std::fill_n(decoded, pixels, std::uint16_t(0));
  context.output.crop_x = (width - 144) / 2;
  context.output.crop_y = (height - 144) / 2;
  return jd_decomp(&decoder, output_cb, BYTE(scale)) == JDR_OK;
}

bool still_current(const Identity &identity) {
  portENTER_CRITICAL(&mux);
  const bool current = request_ready && runtime && runtime->requested == identity;
  portEXIT_CRITICAL(&mux);
  return current && allowed();
}

bool lookup(const Identity &identity) {
  const auto now = now_ms();
  if (runtime && same_media(runtime->positive_identity, identity)) {
    record(stats.cache_hits); return true;
  }
  if (runtime && runtime->negatives.contains(identity, now)) {
    record(stats.negative_hits); return false;
  }
  char url[640]{};
  if (!search_url(identity, url, sizeof url)) return false;
  HttpResult response{};
  bool search_ok = false;
  for (unsigned attempt = 0; attempt < search_attempts; ++attempt) {
    const auto elapsed = esp_timer_get_time() - last_musicbrainz_us;
    if (elapsed < 1000000)
      vTaskDelay(pdMS_TO_TICKS((1000000 - elapsed + 999) / 1000));
    if (!still_current(identity)) return false;
    response = {};
    last_musicbrainz_us = esp_timer_get_time();
    const bool transported = https_get(url, search_limit, response);
    search_ok = transported && response.status == 200 &&
                std::strncmp(response.type, "application/json", 16) == 0;
    if (search_ok) break;
    const bool retryable = transient_search_failure(response.status);
    ESP_LOGW(tag,
             "lookup_rejected stage=search attempt=%u/%u retryable=%u status=%d bytes=%u type=%s",
             attempt + 1, search_attempts, unsigned(retryable), response.status,
             unsigned(response.size), response.type);
    record(stats.network_errors);
    if (!retryable || attempt + 1 == search_attempts) {
      // Deterministic rejection may be cached.  A transient outage must not
      // suppress the same track for the full negative-cache lifetime.
      if (!retryable) runtime->negatives.remember(identity, now_ms());
      return false;
    }
    const unsigned delay_ms = 5000U << attempt;
    for (unsigned waited = 0; waited < delay_ms; waited += 100) {
      vTaskDelay(pdMS_TO_TICKS(100));
      if (!still_current(identity)) return false;
    }
  }
  Candidate candidates[5]{};
  const auto count = parse_musicbrainz(
      {reinterpret_cast<const char *>(encoded), response.size}, candidates, 5);
  std::array<std::array<char, mbid_capacity>, 5> release_groups{};
  const auto release_group_count = rank_release_groups(
      identity, {candidates, count}, release_groups);
  if (!release_group_count) {
    ESP_LOGW(tag, "lookup_rejected stage=selection candidates=%u", unsigned(count));
    record(stats.lookup_rejected); runtime->negatives.remember(identity, now_ms()); return false;
  }
  runtime->positive_identity = {};
  bool image_decoded = false;
  for (std::size_t attempt = 0; attempt < release_group_count; ++attempt) {
    if (!still_current(identity)) return false;
    std::snprintf(url, sizeof url,
                  "https://coverartarchive.org/release-group/%s/front-250",
                  release_groups[attempt].data());
    response = {};
    const bool image_transport = https_get(url, encoded_limit, response);
    const bool image_payload = image_transport && response.status == 200 &&
        std::strncmp(response.type, "image/jpeg", 10) == 0;
    image_decoded = image_payload && decode_jpeg(response.size);
    if (image_decoded) break;
    ESP_LOGW(tag,
             "cover_candidate_rejected attempt=%u count=%u status=%d bytes=%u type=%s",
             unsigned(attempt + 1), unsigned(release_group_count), response.status,
             unsigned(response.size), response.type);
    // A coverless exact release group is expected.  Transport failures are not
    // evidence that another release group is usable, so do not multiply them.
    if (!response.status || (response.status != 404 && response.status != 410))
      break;
  }
  if (!image_decoded) {
    ESP_LOGW(tag,
             "lookup_rejected stage=image_candidates status=%d bytes=%u type=%s",
             response.status, unsigned(response.size), response.type);
    record(stats.image_rejected); runtime->negatives.remember(identity, now_ms()); return false;
  }
  if (!still_current(identity)) return false;
  runtime->positive_identity = identity;
  return true;
}

void worker(void *) {
  for (;;) {
    Identity identity{}; bool present = false;
    portENTER_CRITICAL(&mux);
    if (request_ready && runtime && !completion_ready.load()) {
      identity = runtime->requested; present = true;
    }
    portEXIT_CRITICAL(&mux);
    if (!present || !allowed()) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
    const bool success = lookup(identity);
    portENTER_CRITICAL(&mux);
    if (request_ready && runtime->requested == identity) {
      request_ready = false;
      if (success) { runtime->completed = identity; completion_ready.store(true); increment(stats.completed); }
    } else increment(stats.stale);
    portEXIT_CRITICAL(&mux);
    ESP_LOGI(tag,
             "result=%s source=%u session=%llu media_generation=%llu metadata_revision=%llu stack_min_bytes=%u",
             success ? "cover_ready" : "generic_fallback", unsigned(identity.source),
             static_cast<unsigned long long>(identity.session),
             static_cast<unsigned long long>(identity.media_generation),
             static_cast<unsigned long long>(identity.metadata_revision),
             unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
} // namespace

bool start() {
  if (worker_handle) return true;
  runtime = static_cast<RuntimeState *>(heap_caps_calloc(
      1, sizeof(RuntimeState), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (runtime) runtime = new (runtime) RuntimeState{};
  encoded = static_cast<std::uint8_t *>(heap_caps_malloc(encoded_limit, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  decoded = static_cast<std::uint16_t *>(heap_caps_malloc(pixels * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  jpeg_pool = static_cast<std::uint8_t *>(heap_caps_malloc(jpeg_pool_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!runtime || !encoded || !decoded || !jpeg_pool) {
    heap_caps_free(runtime);
    heap_caps_free(encoded); heap_caps_free(decoded); heap_caps_free(jpeg_pool);
    runtime = nullptr; encoded = nullptr; decoded = nullptr; jpeg_pool = nullptr;
    increment(stats.allocation_failures); return false;
  }
  if (xTaskCreatePinnedToCoreWithCaps(
          worker, "artwork", worker_stack_bytes, nullptr, 1, &worker_handle, 0,
          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    worker_handle = nullptr;
    heap_caps_free(runtime);
    heap_caps_free(encoded); heap_caps_free(decoded); heap_caps_free(jpeg_pool);
    runtime = nullptr; encoded = nullptr; decoded = nullptr; jpeg_pool = nullptr;
    increment(stats.allocation_failures); return false;
  }
  ESP_LOGI(tag, "online_worker=READY encoded_limit=%u decoded=%u negative_cache=%u",
           unsigned(encoded_limit), unsigned(pixels * 2),
           unsigned(runtime->negatives.entries.size()));
  return true;
}

bool submit(const Identity &identity) {
  if (!identity.usable() || !worker_handle || !runtime) return false;
  portENTER_CRITICAL(&mux);
  if (completion_ready.load() && !(runtime->completed == identity))
    completion_ready.store(false);
  const bool duplicate = (request_ready && runtime->requested == identity) ||
                         (completion_ready.load() && runtime->completed == identity);
  if (!duplicate) {
    runtime->requested = identity; request_ready = true; increment(stats.submitted);
  }
  portEXIT_CRITICAL(&mux);
  return !duplicate;
}

bool copy_completion(const Identity &identity, std::span<std::uint16_t> destination) {
  if (!runtime || destination.size() < pixels || !completion_ready.load() ||
      !(runtime->completed == identity))
    return false;
  std::memcpy(destination.data(), decoded, pixels * sizeof(std::uint16_t));
  completion_ready.store(false);
  return true;
}

void cancel() {
  portENTER_CRITICAL(&mux);
  request_ready = false;
  completion_ready.store(false);
  portEXIT_CRITICAL(&mux);
}

Diagnostics diagnostics() {
  portENTER_CRITICAL(&mux);
  auto value = stats;
  portEXIT_CRITICAL(&mux);
  value.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  value.internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  value.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return value;
}
} // namespace artwork::online
