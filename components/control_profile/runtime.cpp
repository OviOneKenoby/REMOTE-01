#include "control_profile/runtime.hpp"
#include "control_profile/profile.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <cstdio>

namespace control_profile {
namespace {
constexpr char tag[] = "UC_PROFILE";
constexpr unsigned stack_bytes = 24576;
struct Region {
  std::size_t payload = 0, heap_bytes = 0, blocks = 0;
  std::size_t peak_payload = 0, peak_heap_bytes = 0, max_request = 0;
};
struct Placement {
  Region internal, external, other;
  std::size_t allocations = 0, releases = 0, failures = 0;
  bool corrupt = false;
} placement;
TaskHandle_t owner_task = nullptr;
bool started = false;

void allocation(void *payload, std::size_t bytes, bool allocated) {
  if (!payload) {
    ++placement.failures;
    return;
  }
  auto &r = esp_ptr_external_ram(payload) ? placement.external
            : esp_ptr_internal(payload)   ? placement.internal
                                          : placement.other;
  auto *base = static_cast<char *>(payload) - sizeof(std::max_align_t);
  const auto block_bytes = heap_caps_get_allocated_size(base);
  if (allocated) {
    ++placement.allocations;
    ++r.blocks;
    r.payload += bytes;
    r.heap_bytes += block_bytes;
    r.peak_payload = std::max(r.peak_payload, r.payload);
    r.peak_heap_bytes = std::max(r.peak_heap_bytes, r.heap_bytes);
    r.max_request = std::max(r.max_request, bytes);
  } else {
    ++placement.releases;
    if (!r.blocks || r.payload < bytes || r.heap_bytes < block_bytes)
      placement.corrupt = true;
    else {
      --r.blocks;
      r.payload -= bytes;
      r.heap_bytes -= block_bytes;
    }
  }
}
void heaps(const char *stage) {
  multi_heap_info_t internal{}, external{};
  heap_caps_get_info(&internal, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heap_caps_get_info(&external, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  ESP_LOGI(tag, "stage=%s INT free=%u min=%u largest=%u allocated_blocks=%u",
           stage, unsigned(internal.total_free_bytes),
           unsigned(internal.minimum_free_bytes),
           unsigned(internal.largest_free_block),
           unsigned(internal.allocated_blocks));
  ESP_LOGI(tag, "stage=%s PSRAM free=%u min=%u largest=%u allocated_blocks=%u",
           stage, unsigned(external.total_free_bytes),
           unsigned(external.minimum_free_bytes),
           unsigned(external.largest_free_block),
           unsigned(external.allocated_blocks));
}
void metrics(const char *stage, unsigned iterations) {
  heaps(stage);
  bool owner = xTaskGetCurrentTaskHandle() == owner_task;
  ESP_LOGI(tag,
           "stage=%s iterations=%u payload_used=%u peak=%u stack_min_bytes=%u "
           "owner_ok=%u",
           stage, iterations, unsigned(Memory::used), unsigned(Memory::peak),
           unsigned(uxTaskGetStackHighWaterMark(nullptr)), unsigned(owner));
  ESP_LOGI(tag,
           "stage=%s INT payload=%u heap=%u blocks=%u peak_payload=%u "
           "peak_heap=%u max_request=%u",
           stage, unsigned(placement.internal.payload),
           unsigned(placement.internal.heap_bytes),
           unsigned(placement.internal.blocks),
           unsigned(placement.internal.peak_payload),
           unsigned(placement.internal.peak_heap_bytes),
           unsigned(placement.internal.max_request));
  ESP_LOGI(tag,
           "stage=%s PSRAM payload=%u heap=%u blocks=%u peak_payload=%u "
           "peak_heap=%u max_request=%u",
           stage, unsigned(placement.external.payload),
           unsigned(placement.external.heap_bytes),
           unsigned(placement.external.blocks),
           unsigned(placement.external.peak_payload),
           unsigned(placement.external.peak_heap_bytes),
           unsigned(placement.external.max_request));
  ESP_LOGI(tag,
           "stage=%s allocations=%u frees=%u failed_allocations=%u "
           "other_blocks=%u other_peak_payload=%u accounting_corrupt=%u",
           stage, unsigned(placement.allocations), unsigned(placement.releases),
           unsigned(placement.failures), unsigned(placement.other.blocks),
           unsigned(placement.other.peak_payload), unsigned(placement.corrupt));
}
class RuntimeClock final : public Clock {
  U64 start_ms = U64(esp_timer_get_time() / 1000);

public:
  U64 now() override { return U64(esp_timer_get_time() / 1000) - start_ms; }
  bool is_owner() const override {
    return xTaskGetCurrentTaskHandle() == owner_task;
  }
  bool wait_until(U64 until) override {
    if (!is_owner())
      return false;
    while (true) {
      auto current = now();
      if (current >= until)
        break;
      auto ms = std::min(U64(50), until - current);
      vTaskDelay(std::max(TickType_t(1), pdMS_TO_TICKS(ms)));
    }
    return true;
  }
};
class RuntimeReporter final : public Reporter {
public:
  bool failed = false;
  unsigned completed = 0;
  void failure(const char *why) override {
    failed = true;
    ESP_LOGE(tag, "CHECK_FAIL %s", why);
  }
  void checkpoint(const char *stage, unsigned iterations,
                  const Session &s) override {
    completed = iterations;
    metrics(stage, iterations);
    ESP_LOGI(tag, "stage=%s dispatches=%u commits=%u store_blocked=%u", stage,
             unsigned(s.mock.dispatches), unsigned(s.journal.commits),
             unsigned(s.store.blocked));
  }
};
void task(void *) {
  owner_task = xTaskGetCurrentTaskHandle();
  // Allow the unchanged 4.5-second factory color task to finish before
  // baseline.
  vTaskDelay(pdMS_TO_TICKS(5000));
  ESP_LOGI(tag,
           "BEGIN development_only=1 reset_reason=%d owner_cpu=%d "
           "stack_bytes=%u HOME_static=1",
           int(esp_reset_reason()), int(xPortGetCoreID()), stack_bytes);
  if (Memory::used || Memory::observer) {
    ESP_LOGE(tag, "FAIL allocator already owned; no runtime created");
    vTaskDelete(nullptr);
    return;
  }
  Memory::observer = allocation;
  metrics("before_runtime", 0);
  RuntimeClock clock;
  RuntimeReporter report;
  void *block =
      heap_caps_malloc(sizeof(Session), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  bool pass = block && esp_ptr_external_ram(block);
  ESP_LOGI(tag,
           "wrapper_bytes=%u wrapper_psram=%u wrapper_internal=%u "
           "malloc_small_threshold=%u malloc_internal_reserve=%u",
           unsigned(sizeof(Session)),
           unsigned(block && esp_ptr_external_ram(block)),
           unsigned(block && esp_ptr_internal(block)),
           unsigned(CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL),
           unsigned(CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL));
  if (pass) {
    char epoch[64];
    std::snprintf(epoch, sizeof epoch, "mock.profile.boot:%08lx",
                  static_cast<unsigned long>(esp_random()));
    auto *session = new (block) Session(Id(epoch));
    metrics("after_runtime_creation", 0);
    pass = !session->store.blocked && clock.is_owner();
    if (pass)
      pass = exercise(*session, clock, report);
    session
        ->~Session(); // all Core/Mock/Store reads and destruction on this owner
  } else
    report.failure("PSRAM runtime allocation");
  heap_caps_free(block);
  metrics("after_cleanup", report.completed);
  const bool balance = !Memory::used && !placement.internal.blocks &&
                       !placement.external.blocks && !placement.other.blocks &&
                       !placement.other.peak_payload &&
                       placement.allocations == placement.releases &&
                       !placement.failures && !placement.corrupt;
  const bool integrity =
      heap_caps_check_integrity(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, true) &&
      heap_caps_check_integrity(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, true);
  pass = pass && !report.failed && balance && integrity && clock.is_owner();
  Memory::observer = nullptr;
  ESP_LOGI(tag,
           "END local_checks=%s balance=%u heap_integrity=%u "
           "completed_iterations=%u phase_a=OWNER_REVIEW_REQUIRED "
           "HOME_integration=0",
           pass ? "PASS" : "FAIL", unsigned(balance), unsigned(integrity),
           report.completed);
  // This stack/TCB is still present in after_cleanup; idle reclaims it after
  // exit.
  vTaskDelete(nullptr);
}
} // namespace
void start() {
  if (started) {
    ESP_LOGE(tag, "FAIL duplicate start");
    return;
  }
  started = true;
  heaps("before_owner_task");
  if (xTaskCreatePinnedToCore(task, "UC_profile", stack_bytes, nullptr, 2,
                              nullptr, 1) != pdPASS)
    ESP_LOGE(tag, "FAIL owner task creation; Phase A blocked");
}
} // namespace control_profile
