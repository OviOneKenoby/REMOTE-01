#include "diagnostics/api.hpp"
#include "control_home/development.hpp"
#include "control_home/power_suspend.hpp"
#include "network_manager/api.hpp"
#include "power_management/api.hpp"
#include "remote_persistence/diagnostic.hpp"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

namespace control_home {
namespace {
Channel channel;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t owner = nullptr;
bool started = false;
std::size_t live = 0, allocations = 0, frees = 0, failures = 0;
bool corrupt = false;
std::uint64_t encoder_transitions = 0, encoder_detents = 0;
std::uint8_t encoder_last_delta = 0;
void observer(void *p, std::size_t bytes, bool allocated) {
  if (xTaskGetCurrentTaskHandle() != owner)
    corrupt = true;
  if (!p) {
    ++failures;
    return;
  }
  if (allocated) {
    ++allocations;
    live += bytes;
  } else {
    ++frees;
    if (live < bytes)
      corrupt = true;
    else
      live -= bytes;
  }
}
void telemetry(const char *stage) {
  multi_heap_info_t internal{}, external{};
  heap_caps_get_info(&internal, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  heap_caps_get_info(&external, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  ESP_LOGI(
      "UC_RUNTIME",
      "stage=%s INT free=%u min=%u largest=%u PSRAM free=%u min=%u largest=%u",
      stage, unsigned(internal.total_free_bytes),
      unsigned(internal.minimum_free_bytes),
      unsigned(internal.largest_free_block),
      unsigned(external.total_free_bytes),
      unsigned(external.minimum_free_bytes),
      unsigned(external.largest_free_block));
  unsigned rejected;
  portENTER_CRITICAL(&mux);
  rejected = unsigned(channel.rejected);
  portEXIT_CRITICAL(&mux);
  ESP_LOGI(
      "UC_RUNTIME",
      "stage=%s payload=%u allocations=%u frees=%u failed_allocations=%u "
      "accounting_corrupt=%u stack_min_bytes=%u owner_ok=%u queue_rejected=%u",
      stage, unsigned(control::Memory::used), unsigned(allocations),
      unsigned(frees), unsigned(failures),
      unsigned(corrupt || live != control::Memory::used),
      unsigned(uxTaskGetStackHighWaterMark(nullptr)),
      unsigned(xTaskGetCurrentTaskHandle() == owner), rejected);
}
bool is_owner(void *) { return xTaskGetCurrentTaskHandle() == owner; }
void task(void *) {
  owner = xTaskGetCurrentTaskHandle();
  vTaskDelay(pdMS_TO_TICKS(5000)); // unchanged factory startup finishes first
  ESP_LOGI("UC_RUNTIME",
           "BEGIN synthetic_only=1 profile=0 owner_cpu=%d stack_bytes=24576",
           int(xPortGetCoreID()));
  if (control::Memory::used || control::Memory::observer) {
    ESP_LOGE("UC_RUNTIME", "FAIL allocator ownership");
    vTaskDelete(nullptr);
    return;
  }
  control::Memory::observer = observer;
  void *block = heap_caps_malloc(sizeof(development::Session),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!block) {
    control::Memory::observer = nullptr;
    ESP_LOGE("UC_RUNTIME", "FAIL runtime allocation");
    vTaskDelete(nullptr);
    return;
  }
  char epoch[64];
  std::snprintf(epoch, sizeof epoch, "home.synthetic.boot:%08lx",
                static_cast<unsigned long>(esp_random()));
  auto *s = new (block) development::Session(control::Id(epoch), is_owner, nullptr);
  auto now = [] { return control::U64(esp_timer_get_time() / 1000); };
  if (!s->initialize(now())) {
    s->~Session();
    heap_caps_free(block);
    control::Memory::observer = nullptr;
    ESP_LOGE("UC_RUNTIME", "FAIL initialization");
    vTaskDelete(nullptr);
    return;
  }
  ESP_LOGI("UC_RUNTIME", "wrapper_bytes=%u wrapper_psram=%u",
           unsigned(sizeof(*s)), unsigned(esp_ptr_external_ram(s)));
  // IDF 5.5.3's nonblocking no-driver VFS read sees only driver-buffered
  // availability, so opening /dev/secondary alone never receives a byte.
  // Install the bounded RX/TX driver; keep logs on the same USB interface.
  usb_serial_jtag_driver_config_t input_config =
      USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  const esp_err_t input_status =
      usb_serial_jtag_is_driver_installed()
          ? ESP_OK
          : usb_serial_jtag_driver_install(&input_config);
  if (input_status == ESP_OK)
    usb_serial_jtag_vfs_use_driver();
  ESP_LOGI("UC_MOCK",
           "DEVELOPMENT_ONLY MOCK_ONLY input=%s init=%s keys=p a u 0 1 v d r - "
           "+ o c s "
           "l n f m",
           input_status == ESP_OK ? "USB_SERIAL_JTAG_DRIVER" : "UNAVAILABLE",
           esp_err_to_name(input_status));
  telemetry("initialized");
#if A010_REMOTE_PERSISTENCE_DIAGNOSTIC
  remote_persistence::diagnostic::announce();
#endif
  development::PowerSuspend suspension;
  ESP_LOGI("A011_RTCP", "configured=%u unconfigured_write=0",
           unsigned(s->remote_mode));
  bool power_attempted = false, resume_warned = false;
  auto publish = [&] {
    auto snapshot = s->snapshot();
    portENTER_CRITICAL(&mux); channel.publish(snapshot); portEXIT_CRITICAL(&mux);
  };
  auto drain_input = [&] {
    char discarded[256];
    if (input_status == ESP_OK) (void)usb_serial_jtag_read_bytes(discarded, sizeof discarded, 0);
  };
  control::U64 acquisition = now(), diagnostic_time=0, diagnostic_session=0, diagnostic_revision=0;
  while (true) {
    const auto time = now();
    const auto ds=s->snapshot();
    if(time-diagnostic_time>=1000 || ds.session!=diagnostic_session || ds.capability_revision!=diagnostic_revision) {
      diagnostics::Core d{}; d.time=time; multi_heap_info_t internal{},external{};
      heap_caps_get_info(&internal,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);heap_caps_get_info(&external,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
      unsigned qr;portENTER_CRITICAL(&mux);qr=unsigned(channel.rejected);portEXIT_CRITICAL(&mux);
      auto q=s->boundary.inbox.counts();const auto &c=s->boundary.counts;
      std::uint64_t transitions,detents;std::uint8_t last_delta;Channel local;
      portENTER_CRITICAL(&mux);transitions=encoder_transitions;detents=encoder_detents;last_delta=encoder_last_delta;local=channel;portEXIT_CRITICAL(&mux);
      const auto &rd=s->rtcp.diagnostic();const auto &rc=rd.counters;
      d.values={ds.session,ds.capability_revision,s->dispatches(),control::Memory::used,allocations,frees,failures,corrupt||live!=control::Memory::used,xTaskGetCurrentTaskHandle()==owner,qr,uxTaskGetStackHighWaterMark(nullptr),internal.total_free_bytes,internal.minimum_free_bytes,internal.largest_free_block,external.total_free_bytes,external.minimum_free_bytes,external.largest_free_block,q.posted,q.rejected,q.overflow,q.busy,c.applied,c.stale,c.invalid,c.refused,c.lifecycle,c.discarded,
        transitions,detents,last_delta,local.volume_attempts,local.volume_accepted,unsigned(local.last_volume_admission),local.latest.connection==Connection::CONNECTED,local.latest.volume_known,local.latest.volume_set,local.latest.volume,local.latest.volume==0,local.latest.volume>=100,local.volume_outstanding};
      const auto q32=[](std::uint32_t value){const auto units=(std::uint64_t(value)+31)/32;return std::uint8_t(units>255?255:units);};
      d.rtcp.set_stage_failure(std::uint8_t(rd.stage),std::uint8_t(rd.failure));d.rtcp.error=std::int16_t(rd.error);d.rtcp.server_hello_bytes=std::uint8_t(rd.server_hello_bytes);d.rtcp.close_reason=std::uint8_t(rd.close_reason);d.rtcp.close_count=std::uint8_t(rd.close_count>255?255:rd.close_count);
      d.rtcp.last_poll_32ms=q32(rd.last_poll_interval_ms);d.rtcp.max_poll_32ms=q32(rd.max_poll_interval_ms);d.rtcp.timeout_elapsed_32ms=q32(rd.last_response_timeout_elapsed_ms);d.rtcp.timeout_state=rd.last_response_timeout_state;d.rtcp.timeout_queued_bytes=rd.last_response_timeout_queued_bytes<0?UINT16_MAX:std::uint16_t(rd.last_response_timeout_queued_bytes>=UINT16_MAX?UINT16_MAX-1:rd.last_response_timeout_queued_bytes);d.rtcp.timeout_unread_count=std::uint8_t(rd.response_timeouts_with_unread>255?255:rd.response_timeouts_with_unread);
      for(std::size_t i=0;i<d.rtcp.counters.size();++i)d.rtcp.counters[i]=std::uint8_t(rc[i]>255?255:rc[i]);
      diagnostics::publish(d);
      if(ds.session!=diagnostic_session || ds.capability_revision!=diagnostic_revision)diagnostics::event(time,diagnostics::Event::CORE,unsigned(ds.session));
      diagnostic_time=time;diagnostic_session=ds.session;diagnostic_revision=ds.capability_revision;
    }
    if (power_management::owner_request() != power_management::Request::NONE) {
      if (!power_attempted) {
        power_attempted = true;
        bool ok = suspension.park(*s, time, power_management::owner_request() == power_management::Request::SHUTDOWN);
        if (ok) { drain_input(); publish(); }
        power_management::owner_ack(ok, !ok);
        ESP_LOGI("POWER", "owner_parked=%u refused=%u pending=%u", unsigned(ok), unsigned(!ok), unsigned(s->pending()));
      }
      vTaskDelay(pdMS_TO_TICKS(20)); continue;
    }
    power_attempted = false;
    if (suspension.parked) {
      if (!suspension.resume(*s, time)) {
        if (!resume_warned) ESP_LOGE("POWER", "owner_resume=FAILED controls_unavailable=1");
        resume_warned = true;
        vTaskDelay(pdMS_TO_TICKS(1000)); continue;
      }
      resume_warned = false; drain_input(); publish(); acquisition = time;
      power_management::owner_ack(false);
      ESP_LOGI("POWER", "owner_resume=READY session=%llu buffered_input_discarded=1",
               static_cast<unsigned long long>(s->snapshot().session));
    }
    s->core.tick(time);
    if (!s->poll(time))
      ESP_LOGW("A011_RTCP", "adapter_event_rejected=1");
    // At most one external inbox event per turn, on the same Core owner.
    s->boundary.process_one(time);
    // Fixed budget: at most eight serial bytes and one HOME intent per turn.
    char keys[8];
    const int bytes = input_status == ESP_OK
                          ? usb_serial_jtag_read_bytes(keys, sizeof keys, 0)
                          : 0;
    for (int i = 0; i < bytes; ++i) {
#if A010_REMOTE_PERSISTENCE_DIAGNOSTIC
      if (remote_persistence::diagnostic::consume_uart_byte(
              static_cast<unsigned char>(keys[i])))
        continue;
#endif
      if (network_manager::development_key(static_cast<unsigned char>(keys[i]))) {
        const bool queued = network_manager::development_command(static_cast<unsigned char>(keys[i]));
        ESP_LOGI("NETWORK_DEV", "key=%c queued=%u DEVELOPMENT_ONLY", keys[i], unsigned(queued));
        continue;
      }
      if (power_management::development_key(static_cast<unsigned char>(keys[i]))) {
        bool queued = power_management::development_command(static_cast<unsigned char>(keys[i]));
        ESP_LOGI("POWER_DEV", "key=%c queued=%u DEVELOPMENT_ONLY owner_path=1", keys[i], unsigned(queued));
        continue;
      }
      const bool is_owner = xTaskGetCurrentTaskHandle() == owner;
      const auto reply = development::input(
          *s, static_cast<unsigned char>(keys[i]), time, is_owner);
      if (reply.kind == development::InputKind::IGNORE)
        continue;
      if (reply.kind == development::InputKind::UNKNOWN) {
        ESP_LOGI("UC_MOCK", "key=%c recognized=0 accepted=0 owner_ok=%u",
                 keys[i], unsigned(is_owner));
      } else {
        if(!reply.accepted)diagnostics::event(time,diagnostics::Event::CORE_REFUSED,unsigned(static_cast<unsigned char>(keys[i])));
        const auto state = s->snapshot();
        ESP_LOGI("UC_MOCK",
                 "key=%c recognized=1 accepted=%u owner_ok=%u session=%llu "
                 "capability_revision=%llu "
                 "dispatches=%u",
                 keys[i], unsigned(reply.accepted), unsigned(is_owner),
                 static_cast<unsigned long long>(state.session),
                 static_cast<unsigned long long>(state.capability_revision),
                 s->dispatches());
        if (reply.kind == development::InputKind::TELEMETRY && reply.accepted)
          {
            telemetry("requested");
            auto q = s->boundary.inbox.counts();
            const auto &c = s->boundary.counts;
            ESP_LOGI("UC_INGRESS", "posted=%u rejected=%u overflow=%u busy=%u applied=%llu stale=%llu invalid=%llu refused=%llu lifecycle=%llu discarded=%llu",
              unsigned(q.posted), unsigned(q.rejected), unsigned(q.overflow), unsigned(q.busy),
              (unsigned long long)c.applied, (unsigned long long)c.stale, (unsigned long long)c.invalid,
              (unsigned long long)c.refused, (unsigned long long)c.lifecycle, (unsigned long long)c.discarded);
          }
      }
    }
    Intent intent;
    bool present;
    portENTER_CRITICAL(&mux);
    present = channel.pop(intent);
    portEXIT_CRITICAL(&mux);
    if (present) {
      bool accepted = s->handle(intent, time);
      if (!accepted) {
        portENTER_CRITICAL(&mux);
        channel.resolve(intent.token, VolumeResolution::DEFINITE_FAILURE);
        portEXIT_CRITICAL(&mux);
      }
      if(!accepted)diagnostics::event(time,diagnostics::Event::CORE_REFUSED,unsigned(intent.action));
      ESP_LOGI("UC_RUNTIME", "intent=%llu action=%u accepted=%u dispatches=%u first_input_to_owner_ms=%llu latest_input_to_owner_ms=%llu target=%u",
               static_cast<unsigned long long>(intent.token),
               unsigned(intent.action), unsigned(accepted),
               s->dispatches(),
               static_cast<unsigned long long>(time >= intent.first_input_ms ? time - intent.first_input_ms : 0),
               static_cast<unsigned long long>(time >= intent.latest_input_ms ? time - intent.latest_input_ms : 0),
               unsigned(intent.volume));
    }
    std::uint64_t outstanding_token;
    portENTER_CRITICAL(&mux);
    outstanding_token = channel.volume_token;
    portEXIT_CRITICAL(&mux);
    if (outstanding_token) {
      const auto resolution = s->volume_resolution(outstanding_token);
      portENTER_CRITICAL(&mux);
      channel.resolve(outstanding_token, resolution);
      portEXIT_CRITICAL(&mux);
    }
    if (time - acquisition >= 1000) {
      s->acquire(time);
      acquisition = time;
    }
    auto snapshot = s->snapshot();
    portENTER_CRITICAL(&mux);
    channel.publish(snapshot, time);
    portEXIT_CRITICAL(&mux);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
} // namespace
bool request(Action action) {
  if (!power_management::commands_allowed()) return false;
  portENTER_CRITICAL(&mux);
  bool accepted = channel.request(action, std::uint64_t(esp_timer_get_time() / 1000));
  portEXIT_CRITICAL(&mux);
  return accepted;
}
void record_encoder_state(std::uint64_t transitions, std::uint64_t detents,
                          std::int8_t last_delta) {
  portENTER_CRITICAL(&mux);
  encoder_transitions = transitions;
  encoder_detents = detents;
  encoder_last_delta = last_delta > 0 ? 2 : last_delta < 0 ? 1 : 0;
  portEXIT_CRITICAL(&mux);
}
bool read(Snapshot &snapshot, std::uint64_t &generation) {
  portENTER_CRITICAL(&mux);
  bool changed = channel.generation != generation;
  if (changed) {
    snapshot = channel.view();
    generation = channel.generation;
  }
  portEXIT_CRITICAL(&mux);
  return changed;
}
void start() {
  if (started)
    return;
  started = true;
  if (xTaskCreatePinnedToCore(task, "UC_owner", 24576, nullptr, 2, nullptr,
                              1) != pdPASS)
    ESP_LOGE("UC_RUNTIME", "FAIL owner creation");
}
} // namespace control_home
