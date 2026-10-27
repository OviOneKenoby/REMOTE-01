#include "diagnostics/api.hpp"
#include "control_home/development.hpp"
#include "control_home/power_suspend.hpp"
#include "control_home/full_control.hpp"
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
std::uint64_t encoder_invalid = 0, encoder_positive = 0,
              encoder_negative = 0, encoder_rejected = 0;
std::uint32_t encoder_sample_last_ms = 0, encoder_sample_max_ms = 0;
std::uint8_t encoder_last_delta = 0;
struct FullControlChannel {
  FullControlSnapshot view{};
  std::uint64_t generation = 1;
  std::uint64_t next_request = 0x46554c4c00000000ULL;
  std::uint64_t pending_request = 0;
  std::uint64_t handled_result = 0;
  bool requested = false;
  TunerSource requested_source = TunerSource::UNKNOWN;
} full_control;
struct MenuChannel {
  MenuSnapshot view{};
  std::uint64_t generation = 1;
  std::uint64_t observed_sequence = 0;
} menu_channel;
struct CatalogChannel {
  enum class Request : std::uint8_t { NONE, QUERY, COMMAND };
  CatalogSnapshot view{};
  std::uint64_t generation = 1;
  std::uint64_t observed_sequence = 0;
  std::uint64_t next_request = 0x4341544100000000ULL;
  std::uint64_t pending_request = 0;
  std::uint64_t handled_result = 0;
  std::uint64_t requested_revision = 0;
  Request request = Request::NONE;
  CatalogCollection collection = CatalogCollection::SAVED;
  CatalogOperation operation = CatalogOperation::PLAY;
  std::uint16_t offset_or_token = 0;
  bool query_in_flight = false;
} catalog_channel;
struct BrowserChannel {
  enum class Request : std::uint8_t { NONE, QUERY, COMMAND };
  BrowserSnapshot view{};
  std::uint64_t generation=1,observed_sequence=0;
  std::uint64_t next_request=0x42524f5700000000ULL,pending_request=0,handled_result=0;
  std::uint64_t requested_generation=0,requested_revision=0;
  Request request=Request::NONE;
  BrowserStage stage=BrowserStage::COUNTRIES;
  BrowserOperation operation=BrowserOperation::START;
  std::uint16_t offset_or_token=0;
  std::uint8_t filter=0;
  bool query_in_flight=false;
  diagnostics::BrowserDiagnostic diagnostic{};
} browser_channel;

void publish_full_control(const FullControlSnapshot &next) {
  const auto &old = full_control.view;
  if (old.connected == next.connected && old.supported == next.supported &&
      old.bluetooth_connected == next.bluetooth_connected &&
      old.command_pending == next.command_pending &&
      old.result_present == next.result_present &&
      old.result_accepted == next.result_accepted &&
      old.last_error == next.last_error && old.source == next.source &&
      old.requested_source == next.requested_source &&
      old.transition == next.transition &&
      old.rtcp_close_reason == next.rtcp_close_reason &&
      old.command_id == next.command_id && old.session == next.session &&
      old.source_generation == next.source_generation) return;
  full_control.view = next;
  if (full_control.generation != UINT64_MAX) ++full_control.generation;
}
void publish_menu(const rtcp_client::wire::MenuState &source, bool connected,
                  bool supported) {
  if (source.sequence && source.sequence == menu_channel.observed_sequence &&
      menu_channel.view.connected == connected &&
      menu_channel.view.supported == supported)
    return;
  MenuSnapshot next{};
  next.connected = connected;
  next.supported = supported;
  if (connected && supported && source.sequence) {
    next.loading = source.loading();
    next.failed = source.failed();
    next.error = source.error;
    next.mode = static_cast<TunerMenuMode>(source.mode);
    next.selected = source.selected;
    next.count = source.count;
    next.window_start = source.window_start;
    next.window_count = source.window_count;
    next.revision = source.revision;
    for (std::uint8_t i = 0; i < source.window_count; ++i) {
      next.items[i].token = source.items[i].token;
      next.items[i].active = source.items[i].active;
      next.items[i].name = source.items[i].name;
    }
  }
  menu_channel.view = next;
  menu_channel.observed_sequence = connected && supported ? source.sequence : 0;
  if (menu_channel.generation != UINT64_MAX) ++menu_channel.generation;
}
void publish_catalog(const rtcp_client::wire::CatalogPage &source,
                     bool connected, bool supported) {
  CatalogSnapshot next = catalog_channel.view;
  next.connected = connected;
  next.supported = supported;
  if (!connected || !supported) {
    next = {};
    next.connected = connected;
    next.supported = supported;
    catalog_channel.observed_sequence = 0;
    catalog_channel.query_in_flight = false;
  } else if (source.sequence && source.sequence != catalog_channel.observed_sequence) {
    next.collection = static_cast<CatalogCollection>(source.collection);
    next.flags = source.flags;
    next.capacity = source.capacity;
    next.item_count = source.item_count;
    next.count = source.count;
    next.start = source.start;
    next.sequence = source.sequence;
    next.revision = source.revision;
    next.items = {};
    for (std::uint8_t i = 0; i < source.item_count; ++i) {
      next.items[i].token = source.items[i].token;
      next.items[i].active = source.items[i].active;
      next.items[i].name = source.items[i].name;
    }
    catalog_channel.observed_sequence = source.sequence;
    catalog_channel.query_in_flight = false;
  }
  next.loading = catalog_channel.request == CatalogChannel::Request::QUERY ||
                 catalog_channel.query_in_flight;
  next.command_pending =
      catalog_channel.request == CatalogChannel::Request::COMMAND ||
      catalog_channel.pending_request != 0;
  if (next.connected != catalog_channel.view.connected ||
      next.supported != catalog_channel.view.supported ||
      next.loading != catalog_channel.view.loading ||
      next.command_pending != catalog_channel.view.command_pending ||
      next.sequence != catalog_channel.view.sequence ||
      next.result_present != catalog_channel.view.result_present ||
      next.result_accepted != catalog_channel.view.result_accepted ||
      next.last_error != catalog_channel.view.last_error) {
    catalog_channel.view = next;
    if (catalog_channel.generation != UINT64_MAX) ++catalog_channel.generation;
  }
}
void publish_browser(const rtcp_client::wire::BrowserPage &source,
                     bool connected,bool supported) {
  BrowserSnapshot next=browser_channel.view;
  next.connected=connected;next.supported=supported;
  if(!connected||!supported){next={};next.connected=connected;next.supported=supported;
    browser_channel.observed_sequence=0;browser_channel.query_in_flight=false;
  }else if(source.sequence&&source.sequence!=browser_channel.observed_sequence){
    diagnostics::BrowserDiagnostic::increment(browser_channel.diagnostic.counters[2]);
    if(browser_channel.query_in_flight&&browser_channel.requested_generation&&
       source.generation!=browser_channel.requested_generation){
      diagnostics::BrowserDiagnostic::increment(browser_channel.diagnostic.counters[4]);
      browser_channel.diagnostic.mismatch_generation_low16=std::uint16_t(source.generation);
      browser_channel.diagnostic.mismatch_revision_low16=std::uint16_t(source.revision);
    }
    diagnostics::BrowserDiagnostic::increment(browser_channel.diagnostic.counters[3]);
    next.stage=static_cast<BrowserStage>(source.stage);
    next.status=static_cast<BrowserStatus>(source.status);next.last_error=source.error;
    next.item_count=source.item_count;next.count=source.count;next.start=source.start;
    next.sequence=source.sequence;next.session_generation=source.generation;next.revision=source.revision;
    next.items={};for(std::uint8_t i=0;i<source.item_count;++i){next.items[i].token=source.items[i].token;next.items[i].active=source.items[i].active;next.items[i].name=source.items[i].name;}
    browser_channel.observed_sequence=source.sequence;browser_channel.query_in_flight=false;
  }
  next.query_pending=browser_channel.request==BrowserChannel::Request::QUERY||browser_channel.query_in_flight;
  next.command_pending=browser_channel.request==BrowserChannel::Request::COMMAND||browser_channel.pending_request;
  const auto &old=browser_channel.view;
  const bool changed=next.connected!=old.connected||next.supported!=old.supported||
    next.query_pending!=old.query_pending||next.command_pending!=old.command_pending||
    next.result_present!=old.result_present||next.result_accepted!=old.result_accepted||
    next.last_error!=old.last_error||next.sequence!=old.sequence;
  if(changed){browser_channel.view=next;if(browser_channel.generation!=UINT64_MAX)++browser_channel.generation;}
}
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
      std::uint64_t transitions,detents,invalid,positive,negative,encoder_rejects;
      std::uint32_t sample_last_ms,sample_max_ms;std::uint8_t last_delta;Channel local;diagnostics::BrowserDiagnostic browser_diagnostic;
      portENTER_CRITICAL(&mux);transitions=encoder_transitions;detents=encoder_detents;invalid=encoder_invalid;positive=encoder_positive;negative=encoder_negative;encoder_rejects=encoder_rejected;sample_last_ms=encoder_sample_last_ms;sample_max_ms=encoder_sample_max_ms;last_delta=encoder_last_delta;local=channel;browser_diagnostic=browser_channel.diagnostic;portEXIT_CRITICAL(&mux);
      const auto &rd=s->rtcp.diagnostic();const auto &rc=rd.counters;
      d.values={ds.session,ds.capability_revision,s->dispatches(),control::Memory::used,allocations,frees,failures,corrupt||live!=control::Memory::used,xTaskGetCurrentTaskHandle()==owner,qr,uxTaskGetStackHighWaterMark(nullptr),internal.total_free_bytes,internal.minimum_free_bytes,internal.largest_free_block,external.total_free_bytes,external.minimum_free_bytes,external.largest_free_block,q.posted,q.rejected,q.overflow,q.busy,c.applied,c.stale,c.invalid,c.refused,c.lifecycle,c.discarded};
      const auto u32=[](std::uint64_t value){return std::uint32_t(value>UINT32_MAX?UINT32_MAX:value);};
      const auto u16=[](std::uint64_t value){return std::uint16_t(value>UINT16_MAX?UINT16_MAX:value);};
      d.control_values={u32(transitions),u32(detents),last_delta,u32(local.volume_attempts),u32(local.volume_accepted),unsigned(local.last_volume_admission),local.latest.connection==Connection::CONNECTED,local.latest.volume_known,local.latest.volume_set,local.latest.volume,local.latest.volume==0,local.latest.volume>=100,local.volume_outstanding};
      d.input_values={u16(invalid),u16(positive),u16(negative),u16(encoder_rejects),u16(sample_last_ms),u16(sample_max_ms),u16(local.volume_queued),u16(local.volume_completed),u16(local.volume_failed),u16(local.volume_cancelled),u16(local.authoritative_updates),u16(local.cancel_session),u16(local.cancel_capability),u16(local.cancel_authority),u16(local.cancel_divergence)};
      const auto q32=[](std::uint32_t value){const auto units=(std::uint64_t(value)+31)/32;return std::uint8_t(units>255?255:units);};
      d.rtcp.set_stage_failure(std::uint8_t(rd.stage),std::uint8_t(rd.failure));d.rtcp.error=std::int16_t(rd.error);d.rtcp.server_hello_bytes=std::uint8_t(rd.server_hello_bytes);d.rtcp.close_reason=std::uint8_t(rd.close_reason);d.rtcp.close_count=std::uint8_t(rd.close_count>255?255:rd.close_count);
      d.rtcp.last_poll_32ms=q32(rd.last_poll_interval_ms);d.rtcp.max_poll_32ms=q32(rd.max_poll_interval_ms);d.rtcp.timeout_elapsed_32ms=q32(rd.last_response_timeout_elapsed_ms);d.rtcp.timeout_state=rd.last_response_timeout_state;d.rtcp.timeout_queued_bytes=rd.last_response_timeout_queued_bytes<0?UINT16_MAX:std::uint16_t(rd.last_response_timeout_queued_bytes>=UINT16_MAX?UINT16_MAX-1:rd.last_response_timeout_queued_bytes);d.rtcp.timeout_unread_count=std::uint8_t(rd.response_timeouts_with_unread>255?255:rd.response_timeouts_with_unread);
      for(std::size_t i=0;i<d.rtcp.counters.size();++i)d.rtcp.counters[i]=std::uint8_t(rc[i]>255?255:rc[i]);
      d.browser=browser_diagnostic;
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
    bool command_waiting;
    portENTER_CRITICAL(&mux);
    command_waiting = channel.count != 0 || full_control.requested ||
                      catalog_channel.request != CatalogChannel::Request::NONE ||
                      browser_channel.request != BrowserChannel::Request::NONE;
    portEXIT_CRITICAL(&mux);
    if (!s->poll(time, command_waiting))
      ESP_LOGW("A011_RTCP", "adapter_event_rejected=1");
    {
      FullControlSnapshot next{};
      const auto &capability = s->rtcp.full_control_capabilities();
      const auto &source = s->rtcp.source_state();
      next.connected = s->rtcp.state() == rtcp_client::Client::State::ready;
      next.supported = capability.source_switch();
      next.source = static_cast<TunerSource>(source.source);
      next.transition = static_cast<SourceTransition>(source.transition);
      next.bluetooth_connected = source.bluetooth_connected;
      next.source_generation = source.generation;
      next.session = s->snapshot().session;
      next.rtcp_close_reason =
          static_cast<std::uint8_t>(s->rtcp.diagnostic().close_reason);
      portENTER_CRITICAL(&mux);
      next.command_pending = full_control.pending_request != 0 || full_control.requested;
      next.requested_source = full_control.pending_request || full_control.requested ||
                                      full_control.view.result_present
                                  ? full_control.requested_source
                                  : TunerSource::UNKNOWN;
      next.result_present = full_control.view.result_present;
      next.result_accepted = full_control.view.result_accepted;
      next.last_error = full_control.view.last_error;
      next.command_id = full_control.pending_request
                            ? full_control.pending_request
                            : full_control.handled_result;
      if (!next.connected) {
        full_control.requested = false;
        full_control.pending_request = 0;
        next.command_pending = false;
        next.requested_source = TunerSource::UNKNOWN;
        next.result_present = false;
        next.result_accepted = false;
        next.last_error = 0;
      }
      const auto &result = s->rtcp.result();
      if (full_control.pending_request &&
          result.request == full_control.pending_request &&
          result.request != full_control.handled_result) {
        full_control.handled_result = result.request;
        full_control.pending_request = 0;
        next.command_pending = false;
        next.result_present = true;
        next.result_accepted = result.accepted;
        next.last_error = result.error;
      }
      publish_full_control(next);
      publish_menu(s->rtcp.menu_state(), next.connected,
                   capability.menu_projection());
      if (!next.connected) {
        catalog_channel.request = CatalogChannel::Request::NONE;
        catalog_channel.pending_request = 0;
        catalog_channel.query_in_flight = false;
        catalog_channel.view.result_present = false;
        catalog_channel.view.result_accepted = false;
        catalog_channel.view.last_error = 0;
      }
      const auto &catalog_result = s->rtcp.result();
      if (catalog_channel.pending_request &&
          catalog_result.request == catalog_channel.pending_request &&
          catalog_result.request != catalog_channel.handled_result) {
        catalog_channel.handled_result = catalog_result.request;
        catalog_channel.pending_request = 0;
        catalog_channel.view.command_pending = false;
        catalog_channel.view.result_present = true;
        catalog_channel.view.result_accepted = catalog_result.accepted;
        catalog_channel.view.last_error = catalog_result.error;
        if (catalog_channel.generation != UINT64_MAX)
          ++catalog_channel.generation;
      }
      publish_catalog(s->rtcp.catalog_page(), next.connected,
                      capability.catalog_query() && capability.catalog_command());
      if (!next.connected) {
        browser_channel.request=BrowserChannel::Request::NONE;
        browser_channel.pending_request=0;browser_channel.query_in_flight=false;
        browser_channel.view.result_present=false;
      }
      const auto &browser_result=s->rtcp.result();
      if(browser_channel.pending_request&&browser_result.request==browser_channel.pending_request&&
         browser_result.request!=browser_channel.handled_result){
        if(browser_channel.view.requested_operation==BrowserOperation::START)
          diagnostics::BrowserDiagnostic::increment(browser_channel.diagnostic.counters[0]);
        browser_channel.handled_result=browser_result.request;browser_channel.pending_request=0;
        browser_channel.view.command_pending=false;browser_channel.view.result_present=true;
        browser_channel.view.result_accepted=browser_result.accepted;
        browser_channel.view.last_error=browser_result.error;
        if(browser_channel.generation!=UINT64_MAX)++browser_channel.generation;
      }
      publish_browser(s->rtcp.browser_page(),next.connected,capability.radio_browser());
      portEXIT_CRITICAL(&mux);
    }
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
    // Read-only RTCP snapshot/metadata exchanges temporarily occupy the
    // single authenticated record lane. Keep the bounded intent queued until
    // that lane is free instead of misclassifying it as a command failure.
    present = channel.pop(intent, s->command_ready());
    portEXIT_CRITICAL(&mux);
    if (present) {
      bool accepted = s->handle(intent, time);
      if (!accepted) {
        portENTER_CRITICAL(&mux);
        channel.resolve(intent.token, VolumeResolution::DEFINITE_FAILURE);
        portEXIT_CRITICAL(&mux);
        if (intent.action == Action::VOLUME_DOWN ||
            intent.action == Action::VOLUME_UP)
          diagnostics::event(time, diagnostics::Event::VOLUME_FAILURE,
                             unsigned(s->last_volume_failure()));
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
    bool source_present = false;
    TunerSource source_target = TunerSource::UNKNOWN;
    portENTER_CRITICAL(&mux);
    if (full_control.requested && !full_control.pending_request) {
      source_present = true;
      source_target = full_control.requested_source;
    }
    portEXIT_CRITICAL(&mux);
    if (source_present && s->command_ready()) {
      const auto wire_target = static_cast<rtcp_client::wire::Source>(source_target);
      const auto generation = s->rtcp.source_state().generation;
      bool sent = false;
      std::uint64_t request = 0;
      portENTER_CRITICAL(&mux);
      if (++full_control.next_request == 0) ++full_control.next_request;
      request = full_control.next_request;
      portEXIT_CRITICAL(&mux);
      if (generation)
        sent = s->rtcp.source_select(request, wire_target, generation);
      portENTER_CRITICAL(&mux);
      if (sent) {
        full_control.requested = false;
        full_control.pending_request = request;
        full_control.view.command_pending = true;
        full_control.view.result_present = false;
        full_control.view.last_error = 0;
        ++full_control.generation;
      }
      portEXIT_CRITICAL(&mux);
    }
    CatalogChannel::Request catalog_request = CatalogChannel::Request::NONE;
    CatalogCollection catalog_collection = CatalogCollection::SAVED;
    CatalogOperation catalog_operation = CatalogOperation::PLAY;
    std::uint16_t catalog_value = 0;
    std::uint64_t catalog_revision = 0;
    portENTER_CRITICAL(&mux);
    catalog_request = catalog_channel.request;
    catalog_collection = catalog_channel.collection;
    catalog_operation = catalog_channel.operation;
    catalog_value = catalog_channel.offset_or_token;
    catalog_revision = catalog_channel.requested_revision;
    portEXIT_CRITICAL(&mux);
    if (catalog_request != CatalogChannel::Request::NONE && s->command_ready()) {
      bool sent = false;
      std::uint64_t request = 0;
      if (catalog_request == CatalogChannel::Request::QUERY) {
        sent = s->rtcp.catalog_query(
            static_cast<rtcp_client::wire::CatalogCollection>(catalog_collection),
            catalog_value, 5);
      } else {
        portENTER_CRITICAL(&mux);
        if (++catalog_channel.next_request == 0) ++catalog_channel.next_request;
        request = catalog_channel.next_request;
        portEXIT_CRITICAL(&mux);
        sent = s->rtcp.catalog_command(
            request,
            static_cast<rtcp_client::wire::CatalogOperation>(catalog_operation),
            static_cast<rtcp_client::wire::CatalogCollection>(catalog_collection),
            catalog_value, catalog_revision);
      }
      portENTER_CRITICAL(&mux);
      if (sent && catalog_channel.request == catalog_request) {
        catalog_channel.request = CatalogChannel::Request::NONE;
        if (catalog_request == CatalogChannel::Request::QUERY)
          catalog_channel.query_in_flight = true;
        else
          catalog_channel.pending_request = request;
        catalog_channel.view.loading = catalog_channel.query_in_flight;
        catalog_channel.view.command_pending =
            catalog_channel.pending_request != 0;
        if (catalog_channel.generation != UINT64_MAX)
          ++catalog_channel.generation;
      }
      portEXIT_CRITICAL(&mux);
    }
    BrowserChannel::Request browser_request=BrowserChannel::Request::NONE;
    BrowserStage browser_stage=BrowserStage::COUNTRIES;BrowserOperation browser_operation=BrowserOperation::START;
    std::uint16_t browser_value=0;std::uint8_t browser_filter=0;
    std::uint64_t browser_session=0,browser_revision=0;
    portENTER_CRITICAL(&mux);
    browser_request=browser_channel.request;browser_stage=browser_channel.stage;
    browser_operation=browser_channel.operation;browser_value=browser_channel.offset_or_token;
    browser_filter=browser_channel.filter;browser_session=browser_channel.requested_generation;
    browser_revision=browser_channel.requested_revision;
    portEXIT_CRITICAL(&mux);
    if(browser_request!=BrowserChannel::Request::NONE&&s->command_ready()){
      bool sent=false;std::uint64_t request=0;
      if(browser_request==BrowserChannel::Request::QUERY)
        sent=s->rtcp.browser_state(static_cast<rtcp_client::wire::BrowserStage>(browser_stage),browser_value,5,browser_filter,browser_session);
      else{
        portENTER_CRITICAL(&mux);if(++browser_channel.next_request==0)++browser_channel.next_request;request=browser_channel.next_request;portEXIT_CRITICAL(&mux);
        sent=s->rtcp.browser_command(request,static_cast<rtcp_client::wire::BrowserOperation>(browser_operation),browser_value,browser_session,browser_revision);
      }
      portENTER_CRITICAL(&mux);
      if(sent&&browser_channel.request==browser_request){browser_channel.request=BrowserChannel::Request::NONE;
        if(browser_request==BrowserChannel::Request::QUERY){browser_channel.query_in_flight=true;
          diagnostics::BrowserDiagnostic::increment(browser_channel.diagnostic.counters[1]);}
        else browser_channel.pending_request=request;
        browser_channel.view.query_pending=browser_channel.query_in_flight;
        browser_channel.view.command_pending=browser_channel.pending_request!=0;
        if(browser_channel.generation!=UINT64_MAX)++browser_channel.generation;}
      portEXIT_CRITICAL(&mux);
    }
    std::uint64_t outstanding_token;
    portENTER_CRITICAL(&mux);
    outstanding_token = channel.volume_token;
    portEXIT_CRITICAL(&mux);
    if (outstanding_token) {
      const auto resolution = s->volume_resolution(outstanding_token);
      if (resolution == VolumeResolution::DEFINITE_FAILURE)
        diagnostics::event(time, diagnostics::Event::VOLUME_FAILURE,
                           unsigned(s->volume_failure(outstanding_token)));
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
                          std::uint64_t invalid, std::uint64_t positive,
                          std::uint64_t negative, std::uint64_t rejected,
                          std::uint32_t sample_last_ms,
                          std::uint32_t sample_max_ms,
                          std::int8_t last_delta) {
  portENTER_CRITICAL(&mux);
  encoder_transitions = transitions;
  encoder_detents = detents;
  encoder_invalid = invalid;
  encoder_positive = positive;
  encoder_negative = negative;
  encoder_rejected = rejected;
  encoder_sample_last_ms = sample_last_ms;
  encoder_sample_max_ms = sample_max_ms;
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
bool full_control_read(FullControlSnapshot &snapshot, std::uint64_t &generation) {
  portENTER_CRITICAL(&mux);
  const bool changed = full_control.generation != generation;
  if (changed) {
    snapshot = full_control.view;
    generation = full_control.generation;
  }
  portEXIT_CRITICAL(&mux);
  return changed;
}
bool menu_read(MenuSnapshot &snapshot, std::uint64_t &generation) {
  portENTER_CRITICAL(&mux);
  const bool changed = menu_channel.generation != generation;
  if (changed) {
    snapshot = menu_channel.view;
    generation = menu_channel.generation;
  }
  portEXIT_CRITICAL(&mux);
  return changed;
}
bool catalog_read(CatalogSnapshot &snapshot, std::uint64_t &generation) {
  portENTER_CRITICAL(&mux);
  const bool changed = catalog_channel.generation != generation;
  if (changed) {
    snapshot = catalog_channel.view;
    generation = catalog_channel.generation;
  }
  portEXIT_CRITICAL(&mux);
  return changed;
}
bool browser_read(BrowserSnapshot &snapshot,std::uint64_t &generation){
  portENTER_CRITICAL(&mux);const bool changed=browser_channel.generation!=generation;
  if(changed){snapshot=browser_channel.view;generation=browser_channel.generation;}
  portEXIT_CRITICAL(&mux);return changed;
}
bool request_source(TunerSource source) {
  if (!power_management::commands_allowed() ||
      (source != TunerSource::RADIO && source != TunerSource::BLUETOOTH))
    return false;
  portENTER_CRITICAL(&mux);
  const bool accepted = full_control.view.connected &&
                        full_control.view.supported &&
                        !full_control.requested &&
                        !full_control.pending_request &&
                        catalog_channel.request == CatalogChannel::Request::NONE &&
                        !catalog_channel.pending_request &&
                        !catalog_channel.query_in_flight &&
                        browser_channel.request==BrowserChannel::Request::NONE &&
                        !browser_channel.pending_request&&!browser_channel.query_in_flight;
  if (accepted) {
    full_control.requested = true;
    full_control.requested_source = source;
    full_control.view.command_pending = true;
    full_control.view.requested_source = source;
    full_control.view.result_present = false;
    full_control.view.last_error = 0;
    ++full_control.generation;
  }
  portEXIT_CRITICAL(&mux);
  return accepted;
}
bool request_catalog(CatalogCollection collection, std::uint16_t offset) {
  if (!power_management::commands_allowed() ||
      collection < CatalogCollection::SAVED ||
      collection > CatalogCollection::RECENT)
    return false;
  portENTER_CRITICAL(&mux);
  const bool accepted = catalog_channel.view.connected &&
                        catalog_channel.view.supported &&
                        catalog_channel.request == CatalogChannel::Request::NONE &&
                        !catalog_channel.pending_request &&
                        !catalog_channel.query_in_flight &&
                        !full_control.requested && !full_control.pending_request &&
                        browser_channel.request==BrowserChannel::Request::NONE &&
                        !browser_channel.pending_request&&!browser_channel.query_in_flight;
  if (accepted) {
    catalog_channel.request = CatalogChannel::Request::QUERY;
    catalog_channel.collection = collection;
    catalog_channel.offset_or_token = offset;
    catalog_channel.view.loading = true;
    catalog_channel.view.result_present = false;
    catalog_channel.view.last_error = 0;
    if (catalog_channel.generation != UINT64_MAX) ++catalog_channel.generation;
  }
  portEXIT_CRITICAL(&mux);
  return accepted;
}
bool request_catalog_action(CatalogOperation operation,
                            CatalogCollection collection,
                            std::uint16_t token,
                            std::uint64_t revision) {
  const bool compatible =
      operation == CatalogOperation::PLAY ||
      (operation == CatalogOperation::REMOVE &&
       collection != CatalogCollection::RECENT) ||
      (operation == CatalogOperation::ADD_FAVORITE &&
       collection == CatalogCollection::RECENT);
  if (!power_management::commands_allowed() || !revision || !compatible ||
      collection < CatalogCollection::SAVED ||
      collection > CatalogCollection::RECENT)
    return false;
  portENTER_CRITICAL(&mux);
  const bool accepted = catalog_channel.view.connected &&
                        catalog_channel.view.supported &&
                        catalog_channel.request == CatalogChannel::Request::NONE &&
                        !catalog_channel.pending_request &&
                        !catalog_channel.query_in_flight &&
                        !full_control.requested && !full_control.pending_request &&
                        browser_channel.request==BrowserChannel::Request::NONE &&
                        !browser_channel.pending_request&&!browser_channel.query_in_flight;
  if (accepted) {
    catalog_channel.request = CatalogChannel::Request::COMMAND;
    catalog_channel.collection = collection;
    catalog_channel.operation = operation;
    catalog_channel.offset_or_token = token;
    catalog_channel.requested_revision = revision;
    catalog_channel.view.command_pending = true;
    catalog_channel.view.requested_operation = operation;
    catalog_channel.view.requested_token = token;
    catalog_channel.view.result_present = false;
    catalog_channel.view.last_error = 0;
    if (catalog_channel.generation != UINT64_MAX) ++catalog_channel.generation;
  }
  portEXIT_CRITICAL(&mux);
  return accepted;
}
bool request_browser_start(){
  if(!power_management::commands_allowed())return false;
  portENTER_CRITICAL(&mux);const bool accepted=browser_channel.view.connected&&browser_channel.view.supported&&
    browser_channel.request==BrowserChannel::Request::NONE&&!browser_channel.pending_request&&!browser_channel.query_in_flight&&
    catalog_channel.request==CatalogChannel::Request::NONE&&!catalog_channel.pending_request&&!full_control.requested&&!full_control.pending_request;
  if(accepted){browser_channel.request=BrowserChannel::Request::COMMAND;browser_channel.operation=BrowserOperation::START;
    browser_channel.offset_or_token=0;browser_channel.requested_generation=0;browser_channel.requested_revision=0;
    browser_channel.view.command_pending=true;browser_channel.view.result_present=false;
    browser_channel.view.requested_operation=BrowserOperation::START;++browser_channel.generation;}
  portEXIT_CRITICAL(&mux);return accepted;
}
bool request_browser_page(BrowserStage stage,std::uint16_t offset,std::uint8_t filter,std::uint64_t generation){
  if(!power_management::commands_allowed()||stage<BrowserStage::COUNTRIES||stage>BrowserStage::RESULTS||
     (stage==BrowserStage::COUNTRIES?(filter&& (filter<'A'||filter>'Z')):filter)||(!generation&&stage!=BrowserStage::COUNTRIES))return false;
  portENTER_CRITICAL(&mux);const bool accepted=browser_channel.view.connected&&browser_channel.view.supported&&
    browser_channel.request==BrowserChannel::Request::NONE&&!browser_channel.pending_request&&!browser_channel.query_in_flight;
  if(accepted){browser_channel.request=BrowserChannel::Request::QUERY;browser_channel.stage=stage;
    browser_channel.offset_or_token=offset;browser_channel.filter=filter;browser_channel.requested_generation=generation;
    browser_channel.view.query_pending=true;++browser_channel.generation;}
  portEXIT_CRITICAL(&mux);return accepted;
}
bool request_browser_action(BrowserOperation operation,std::uint16_t token,std::uint64_t generation,std::uint64_t revision){
  if(!power_management::commands_allowed()||operation<BrowserOperation::SELECT_COUNTRY||operation>BrowserOperation::CANCEL||
     !generation||(operation!=BrowserOperation::CANCEL&&!revision))return false;
  portENTER_CRITICAL(&mux);const bool accepted=browser_channel.view.connected&&browser_channel.view.supported&&
    browser_channel.request==BrowserChannel::Request::NONE&&!browser_channel.pending_request&&!browser_channel.query_in_flight;
  if(accepted){browser_channel.request=BrowserChannel::Request::COMMAND;browser_channel.operation=operation;
    browser_channel.offset_or_token=token;browser_channel.requested_generation=generation;
    browser_channel.requested_revision=operation==BrowserOperation::CANCEL?0:revision;
    browser_channel.view.command_pending=true;browser_channel.view.result_present=false;
    browser_channel.view.requested_operation=operation;browser_channel.view.requested_token=token;++browser_channel.generation;}
  portEXIT_CRITICAL(&mux);return accepted;
}
void browser_ui_diagnostic(std::uint8_t state,BrowserOperation pending_operation){
  portENTER_CRITICAL(&mux);
  browser_channel.diagnostic.ui_state=state;
  browser_channel.diagnostic.pending_operation=std::uint8_t(pending_operation);
  portEXIT_CRITICAL(&mux);
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
