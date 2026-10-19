#include "diagnostics/api.hpp"
#include "power_management/api.hpp"
#include "esp_system.h"
#include "driver/usb_serial_jtag.h"
#include "network_manager/api.hpp"
#include "internal.hpp"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <cstdio>
namespace network_manager {
namespace {
ESP_EVENT_DEFINE_BASE(REMOTE_SLEEP_EVENTS);
Inbox inbox;
sleep::Channel sleep_channel;
sleep::Owner sleep_owner; // existing NETWORK is the sole consumer
std::atomic<bool> driver_events{true};
std::atomic<unsigned> sleep_barrier_ack{0};
std::atomic<unsigned> generation{0}, rejected{0};
std::atomic<bool> submitting{false}, scanning{false};
SemaphoreHandle_t shared_mutex = nullptr;
WebView published;
LocalSetup setup;
SettingsView settings;
std::atomic<TaskHandle_t> owner{nullptr};
wifi_ap_record_t records[16]{}; // Fixed owner-only scan storage, not task stack.
std::uint64_t now() { return esp_timer_get_time() / 1000; }
bool post(Message &m) { bool ok = inbox.post(m); if (!ok) ++rejected; if (auto task = owner.load()) xTaskNotifyGive(task); return ok; }
struct Storage {
  nvs_handle_t handle = 0;
  bool write(const char *, const char *key, const void *data, std::size_t n) {
    return handle && nvs_set_blob(handle, key, data, n) == ESP_OK && nvs_commit(handle) == ESP_OK;
  }
  bool erase(const char *, const char *key) {
    if (!handle) return false;
    auto e = nvs_erase_key(handle, key);
    return (e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND) && nvs_commit(handle) == ESP_OK;
  }
};
void event(void *, esp_event_base_t base, int32_t id, void *data) {
  if(base==REMOTE_SLEEP_EVENTS) {
    if(data)sleep_barrier_ack.store(*static_cast<unsigned*>(data));
    if(auto task=owner.load())xTaskNotifyGive(task);
    return;
  }
  if(!driver_events.load())return;
  if ((base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) || base == IP_EVENT) diagnostics::revoke();
  Message m{}; m.generation = generation.load();
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) m.kind = Kind::GOT_IP;
  else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) { m.kind = Kind::DISCONNECTED; m.detail = 65536; }
  else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    m.kind = Kind::DISCONNECTED;
    if (data) m.detail = static_cast<wifi_event_sta_disconnected_t *>(data)->reason;
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) m.kind = Kind::SCAN_DONE;
  else return;
  post(m);
}
void random_hex(char *out, unsigned bytes) {
  unsigned char b[16]{}; esp_fill_random(b, bytes);
  constexpr char digits[] = "0123456789ABCDEF";
  for (unsigned i = 0; i < bytes; ++i) { out[2*i] = digits[b[i] >> 4]; out[2*i+1] = digits[b[i] & 15]; }
  out[2*bytes] = 0; wipe(b, sizeof b);
}
void task(void *) {
  owner = xTaskGetCurrentTaskHandle();
  vTaskDelay(pdMS_TO_TICKS(6000)); // Local HOME/Core starts first; networking never gates it.
  diagnostics::initialize();
  diagnostics::Reboot reboot; std::uint64_t diagnostic_time=0; unsigned diagnostic_state=~0u, diagnostic_errors=0;
  Policy policy; Storage storage; Credentials credentials{};
  Recovery recovery;
  WebView view{}; LocalSetup local{};
  esp_netif_t *sta = nullptr, *ap = nullptr;
  esp_event_handler_instance_t wifi_handler{}, ip_handler{}, barrier_handler{};
  unsigned barrier_sequence=0;
  bool ready = false, wifi_started = false, wifi_initialized = false;
  std::uint64_t connect_deadline = 0, close_portal = 0, armed_until = 0, scan_deadline = 0;
  unsigned previous_rejected = 0;
  auto publish = [&] {
    view.status.state = policy.state; view.status.portal = policy.portal;
    view.status.connected = policy.state == State::CONNECTED;
    view.status.stored = policy.stored; view.status.generation = policy.generation;
    view.status.reconnects = policy.reconnects; view.status.scanning = scanning.load();
    view.status.rejected = rejected.load(); view.status.stack_min = uxTaskGetStackHighWaterMark(nullptr);
    view.status.http_stack_min = web_stack_min();
    view.status.services = web_stats(); dns_stats(view.status.services);
    local.active = policy.portal && ready;
    generation.store(policy.generation);
    if (xSemaphoreTake(shared_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      published = view; setup = local;
      settings = {policy.generation, recovery.ticket, recovery.deadline, recovery.result};
      xSemaphoreGive(shared_mutex);
    } else ++view.status.errors;
  };
  auto configure_sta = [&] {
    wifi_config_t c{};
    std::memcpy(c.sta.ssid, credentials.ssid, std::strlen(credentials.ssid));
    std::memcpy(c.sta.password, credentials.password, std::strlen(credentials.password));
    c.sta.threshold.authmode = credentials.security == 3 ? WIFI_AUTH_WPA3_PSK : WIFI_AUTH_WPA2_PSK;
    c.sta.pmf_cfg.capable = true; c.sta.pmf_cfg.required = credentials.security == 3;
    c.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    bool ok = esp_wifi_set_config(WIFI_IF_STA, &c) == ESP_OK; wipe(&c, sizeof c); return ok;
  };
  auto connect = [&] {
    connect_deadline = now() + 30000;
    if (!configure_sta() || esp_wifi_connect() != ESP_OK) { ++view.status.errors; policy.failed(now()); submitting.store(false); }
  };
  auto portal = [&] {
    diagnostics::service({});
    if (wifi_started) {
      esp_wifi_scan_stop(); esp_wifi_clear_ap_list();
      if (esp_wifi_stop() != ESP_OK) return false;
    }
    scanning.store(false); scan_deadline = 0; wifi_started = false; web_stop(); dns_stop();
    view.status.scan_count = 0; view.status.ip[0] = 0; view.status.rssi = 0;
    for (auto &network : view.networks) network = {};
    // SDK hardware RNG entropy is guaranteed while Wi-Fi RF is enabled.
    // Start STA without connecting, generate the secrets, then configure AP
    // while stopped: there is never a transient open/default AP.
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK || esp_wifi_start() != ESP_OK) return false;
    wifi_started = true;
    random_hex(local.key, 8); random_hex(view.token, 16); ++local.show;
    if (esp_wifi_stop() != ESP_OK) return false;
    wifi_started = false;
    unsigned char mac[6]{}; if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK) return false;
    std::snprintf(local.ssid, sizeof local.ssid, "REMOTE-01-Setup-%02X%02X", mac[4], mac[5]);
    wifi_config_t c{}; std::memcpy(c.ap.ssid, local.ssid, std::strlen(local.ssid));
    c.ap.ssid_len = std::strlen(local.ssid); std::memcpy(c.ap.password, local.key, 16);
    c.ap.authmode = WIFI_AUTH_WPA2_PSK; c.ap.max_connection = 1; c.ap.channel = 1;
    bool ok = esp_wifi_set_mode(WIFI_MODE_APSTA) == ESP_OK && esp_wifi_set_config(WIFI_IF_AP, &c) == ESP_OK;
    wipe(&c, sizeof c);
    if (!ok || esp_wifi_start() != ESP_OK) return false;
    wifi_started = true;
    return dns_start() && web_start();
  };
  auto healthy = [&] {
    wifi_ap_record_t ap{}; esp_netif_ip_info_t ip{};
    if (!sta || esp_wifi_sta_get_ap_info(&ap) != ESP_OK || !esp_netif_is_netif_up(sta) ||
        esp_netif_get_ip_info(sta, &ip) != ESP_OK || !ip.ip.addr ||
        std::strncmp(reinterpret_cast<const char *>(ap.ssid), credentials.ssid, 32)) {
      view.status.ip[0] = 0; view.status.rssi = 0; return false;
    }
    view.status.rssi = ap.rssi;
    std::snprintf(view.status.ip, sizeof view.status.ip, IPSTR, IP2STR(&ip.ip)); return true;
  };
  auto got_ip = [&] {
    if (!healthy() || (policy.state != State::CONNECTING && policy.state != State::RETRY_WAIT)) return;
    if (policy.candidate && !replace(storage, credentials)) {
      ++view.status.errors; esp_wifi_disconnect(); policy.failed(now()); submitting.store(false); return;
    }
    policy.connected(); submitting.store(false); connect_deadline = 0;
    if (policy.portal) close_portal = now() + 2000;
  };
  // Never erase the partition to "repair" NVS; unrelated state must survive.
  if (nvs_flash_init() == ESP_OK && nvs_open("remote_wifi", NVS_READWRITE, &storage.handle) == ESP_OK) {
    view.status.storage_ready = true;
    Blob blob{}; std::size_t len = blob.size();
    bool have = nvs_get_blob(storage.handle, "credentials", blob.data(), &len) == ESP_OK && len == blob.size() && decode(blob, credentials);
    wipe(blob.data(), blob.size()); policy.boot(have);
    auto net = esp_netif_init(); auto loop = esp_event_loop_create_default();
    if ((net == ESP_OK || net == ESP_ERR_INVALID_STATE) && (loop == ESP_OK || loop == ESP_ERR_INVALID_STATE)) {
      // The convenience create_default helpers assert on allocation failure.
      // Compose the same SDK facilities with checked returns instead.
      esp_netif_config_t sta_config = ESP_NETIF_DEFAULT_WIFI_STA();
      esp_netif_config_t ap_config = ESP_NETIF_DEFAULT_WIFI_AP();
      sta = esp_netif_new(&sta_config); ap = esp_netif_new(&ap_config);
      wifi_init_config_t c = WIFI_INIT_CONFIG_DEFAULT(); c.nvs_enable = 0;
      wifi_initialized = sta && ap && esp_netif_attach_wifi_station(sta) == ESP_OK &&
        esp_netif_attach_wifi_ap(ap) == ESP_OK && esp_wifi_set_default_wifi_sta_handlers() == ESP_OK &&
        esp_wifi_set_default_wifi_ap_handlers() == ESP_OK && esp_wifi_init(&c) == ESP_OK;
      ready = wifi_initialized && esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK &&
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, nullptr, &wifi_handler) == ESP_OK &&
        esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, event, nullptr, &ip_handler) == ESP_OK &&
        esp_event_handler_instance_register(REMOTE_SLEEP_EVENTS,0,event,nullptr,&barrier_handler)==ESP_OK;
      if (ready) {
        if (have) { ready = esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK && esp_wifi_start() == ESP_OK;
          wifi_started = ready; if (ready) connect();
        } else ready = portal();
      }
    }
  }
  if (!ready) {
    ++view.status.errors; policy.stop(); web_stop(); dns_stop();
    if (wifi_handler) esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler);
    if (ip_handler) esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, ip_handler);
    if(barrier_handler)esp_event_handler_instance_unregister(REMOTE_SLEEP_EVENTS,0,barrier_handler);
    if (wifi_started) esp_wifi_stop();
    if (wifi_initialized) esp_wifi_deinit();
    if (sta) esp_netif_destroy_default_wifi(sta);
    if (ap) esp_netif_destroy_default_wifi(ap);
    sta = ap = nullptr;
    wipe(&credentials, sizeof credentials); wipe(&local, sizeof local); wipe(view.token, sizeof view.token);
  }
  publish();
  ESP_LOGI("NETWORK", "STAGE2A DEVELOPMENT_ONLY ready=%u state=%s keys=W G Y R no_secret_logs=1", unsigned(ready), name(policy.state));
  auto fence = [&]() __attribute__((noinline)) {
    if(barrier_sequence==UINT32_MAX)return false;
    const unsigned ticket=++barrier_sequence;
    if(esp_event_post(REMOTE_SLEEP_EVENTS,0,&ticket,sizeof ticket,0)!=ESP_OK)return false;
    const auto end=now()+1000;
    while(sleep_barrier_ack.load()!=ticket && now()<end)ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(10));
    return sleep_barrier_ack.load()==ticket;
  };
  auto sleep_stop = [&]() __attribute__((noinline)) {
    if(!inbox.seal())return false; // atomically refuse pending/new messages; erase nothing
    driver_events.store(false);
    ++policy.generation;generation.store(policy.generation);
    policy.state=State::DISABLED;policy.deadline=0;
    connect_deadline=close_portal=0;view.status.ip[0]=0;view.status.rssi=0;publish();
    const bool ok=sleep::quiesce(
      []{diagnostics::revoke();},[]{return diagnostics::quiesce();},
      []{web_stop();return !web_stats().http;},[]{dns_stop();return !dns_active();},
      [&]{if(!wifi_started)return true;const bool stopped=esp_wifi_stop()==ESP_OK;if(stopped)wifi_started=false;return stopped;},
      fence);
    publish();ESP_LOGI("NETWORK_SLEEP","prepare=%s generation=%u stored=%u",ok?"READY":"FAILED",policy.generation,policy.stored);
    return ok;
  };
  auto sleep_start = [&]() __attribute__((noinline)) {
    if(driver_events.load() && wifi_started)return true; // seal refusal changed no service
    if(!driver_events.load()) {
      // Default-event-loop FIFO barrier drains callbacks from the stopped driver
      // before reopening producer admission with the new generation.
      if(!fence() || policy.generation==UINT32_MAX || !inbox.unseal())return false;
      ++policy.generation;generation.store(policy.generation);driver_events.store(true);
    }
    if(!wifi_started) {
      if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK || esp_wifi_start()!=ESP_OK)return false;
      wifi_started=true;
    }
    policy.state=State::CONNECTING;policy.retry=0;policy.deadline=0;
    connect();publish(); // existing config/connect/deadline/backoff; no NVS writes
    ESP_LOGI("NETWORK_SLEEP","resume=STARTED generation=%u stored=%u",policy.generation,policy.stored);
    return true;
  };
  struct SleepPort {
    decltype(sleep_stop)&stop_fn;decltype(sleep_start)&start_fn;
    bool stop(){return stop_fn();}bool resume(){return start_fn();}
  } sleep_port{sleep_stop,sleep_start};
  for (;;) {
    if(armed_until && now()>armed_until)armed_until=0; // existing 10s confirmation expiry
    const bool can_sleep=sleep::eligible(ready,policy.stored,policy.portal,policy.candidate,
      scanning.load(),submitting.load(),recovery.ticket || armed_until,reboot.pending,policy.generation,barrier_sequence);
    sleep_owner.tick(sleep_channel,sleep_port,can_sleep,now());
    if(sleep_owner.paused()) {ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(50));continue;}
    dns_tick();
    Message m{}; bool received = inbox.pop(m);
    const auto time = now();
    if (reboot.pending && power_management::owner_request()!=power_management::Request::NONE) {
      reboot.execute(time,true,false); diagnostics::event(time,diagnostics::Event::REBOOT_REFUSED);
      ESP_LOGW("NETWORK_DEV","reboot=REFUSED power_pending=1 DEVELOPMENT_ONLY");
    }
    if (reboot.execute(time,power_management::owner_request()!=power_management::Request::NONE,false)) {
      diagnostics::event(time,diagnostics::Event::REBOOT_EXECUTE);
      ESP_LOGW("NETWORK_DEV","reboot=EXECUTING SOFTWARE DEVELOPMENT_ONLY preserve_nvs=1");
      if(usb_serial_jtag_is_driver_installed()) (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(20));
      esp_restart();
    }
    if (received) {
      if (m.kind == Kind::REBOOT) {
        const auto decision=reboot.request(time,power_management::owner_request()!=power_management::Request::NONE,false);
        diagnostics::event(time,decision==diagnostics::Reboot::Result::REFUSE?diagnostics::Event::REBOOT_REFUSED:diagnostics::Event::REBOOT_REQUEST,unsigned(decision));
        ESP_LOGW("NETWORK_DEV","key=R reboot=%s DEVELOPMENT_ONLY defer_ms=500 preserve_nvs=1",decision==diagnostics::Reboot::Result::ACCEPT?"ACCEPTED":decision==diagnostics::Reboot::Result::COALESCE?"COALESCED":"REFUSED");
      }
      else if (m.kind == Kind::SETTINGS) {
        const auto action = static_cast<SettingsAction>(m.detail);
        if (!ready || reboot.pending || power_management::owner_request()!=power_management::Request::NONE) {
          recovery.invalidate(); recovery.result = SettingsResult::REFUSED;
        } else if (recovery.authorize(action,m.generation,policy.generation,m.confirmation,time)) {
          bool change = false;
          if (action == SettingsAction::OPEN_SETUP) {
            if (!policy.portal || policy.state != State::PROVISIONING) { policy.open_setup(); change = true; }
          } else if (action == SettingsAction::CONFIRM_WIFI_RESET) {
            if (erase(storage)) { policy.forget(); wipe(&credentials,sizeof credentials); change = true; }
            else recovery.result = SettingsResult::STORAGE_ERROR;
          } else if (action == SettingsAction::CANCEL_SETUP) {
            Blob saved{}; Credentials previous{}; std::size_t length = saved.size();
            const bool have = storage.handle && nvs_get_blob(storage.handle,"credentials",saved.data(),&length)==ESP_OK &&
              length==saved.size() && decode(saved,previous);
            if (!policy.portal || !have) recovery.result = SettingsResult::REFUSED;
            else {
              diagnostics::service({}); web_stop(); dns_stop();
              esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); scanning.store(false); scan_deadline = 0;
              const bool stopped = !wifi_started || esp_wifi_stop()==ESP_OK;
              if (stopped) wifi_started = false;
              if (stopped && esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK && esp_wifi_start()==ESP_OK) {
                wifi_started = true; credentials = previous; policy.boot(true);
                local.active = false; wipe(local.key,sizeof local.key); wipe(view.token,sizeof view.token);
                submitting.store(false); close_portal = connect_deadline = 0; connect();
              } else { recovery.result = SettingsResult::SERVICE_ERROR; policy.stop(); ready = false; }
            }
            wipe(saved.data(),saved.size()); wipe(&previous,sizeof previous);
          }
          if (change) {
            generation.store(policy.generation); submitting.store(false); close_portal = connect_deadline = 0;
            ready = portal();
            if (!ready) { recovery.result = SettingsResult::SERVICE_ERROR; policy.stop(); web_stop(); dns_stop(); esp_wifi_stop(); }
          }
        }
        if (recovery.result==SettingsResult::STORAGE_ERROR || recovery.result==SettingsResult::SERVICE_ERROR) ++view.status.errors;
        ESP_LOGI("NETWORK_SETTINGS","result=%u generation=%u wifi_only=1",unsigned(recovery.result),policy.generation);
      }
      else if (m.kind == Kind::FAULT) { ++view.status.errors; if (m.detail) ++view.status.allocation_errors; }
      else if (m.kind == Kind::STATUS) {
        if (policy.state == State::CONNECTED && !healthy()) policy.failed(time);
        ++local.show; publish();
        char line[512]{}; diagnostic(view.status, line, sizeof line);
        ESP_LOGI("NETWORK", "%s INT_free=%u PSRAM_free=%u", line,
          unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
          unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        portal::diagnostic(view.status.services,line,sizeof line);
        ESP_LOGI("NETWORK_PORTAL", "%s",line);
        ESP_LOGI("DIAGNOSTICS", "errors=%u stack_min_bytes=%u",diagnostics::errors(),diagnostics::stack_min());
      } else if (ready && m.kind == Kind::ARM_CLEAR) {
        armed_until = time + 10000; ESP_LOGW("NETWORK", "DEVELOPMENT_ONLY clear_armed_ms=10000 confirm=Y wifi_namespace_only=1");
      } else if (ready && m.kind == Kind::CONFIRM_CLEAR) {
        if (armed_until && time <= armed_until && erase(storage)) {
          policy.forget(); generation.store(policy.generation); wipe(&credentials, sizeof credentials);
          submitting.store(false); close_portal = connect_deadline = 0;
          ready = portal(); if (!ready) { ++view.status.errors; policy.stop(); web_stop(); dns_stop(); esp_wifi_stop(); }
          ESP_LOGW("NETWORK", "DEVELOPMENT_ONLY credentials_cleared=1 namespace=remote_wifi");
        } else { ++view.status.errors; ESP_LOGW("NETWORK", "clear=REFUSED expired_or_storage_failure=1"); }
        armed_until = 0;
      } else if (ready && m.generation == policy.generation) {
        if (m.kind == Kind::SUBMIT) {
          if (!scanning.load() && valid(m.credentials) && policy.submit(time)) {
            credentials = m.credentials; generation.store(policy.generation); esp_wifi_disconnect(); connect();
          } else submitting.store(false);
        } else if (m.kind == Kind::SCAN && policy.portal && !policy.candidate) {
          wifi_scan_config_t c{}; c.show_hidden = true;
          scan_deadline = time + 15000;
          if (esp_wifi_scan_start(&c, false) != ESP_OK) { ++view.status.errors; scanning.store(false); }
        } else if (m.kind == Kind::SCAN_DONE && scanning.load()) {
          uint16_t count = 16;
          if (esp_wifi_scan_get_ap_records(&count, records) == ESP_OK) {
            view.status.scan_count = count;
            for (unsigned i = 0; i < 16; ++i) {
              view.networks[i] = {};
              if (i < count) {
                std::memcpy(view.networks[i].ssid, records[i].ssid, 32);
                view.networks[i].rssi = records[i].rssi;
                auto a = records[i].authmode;
                view.networks[i].security = a == WIFI_AUTH_WPA3_PSK ? 3 : a == WIFI_AUTH_WPA2_PSK || a == WIFI_AUTH_WPA2_WPA3_PSK || a == WIFI_AUTH_WPA_WPA2_PSK ? 2 : 0;
              }
            }
          } else { ++view.status.errors; esp_wifi_clear_ap_list(); }
          scanning.store(false); scan_deadline = 0;
        } else if (m.kind == Kind::GOT_IP) got_ip();
        else if (m.kind == Kind::DISCONNECTED &&
                 (m.detail!=65536 || policy.state==State::CONNECTED) && !healthy()) {
          view.status.reason = m.detail; view.status.ip[0] = 0;
          // During CONNECTING the bounded deadline owns failure. A delayed
          // disconnect from the previous attempt must not cancel a new one.
          if (policy.state == State::CONNECTED) { policy.failed(time); submitting.store(false); }
        } else if (m.kind == Kind::SCAN) scanning.store(false);
      } else if (m.kind == Kind::SUBMIT) submitting.store(false);
      else if (m.kind == Kind::SCAN) scanning.store(false);
      wipe(&m, sizeof m); publish();
    }
    if (ready && policy.state == State::CONNECTING && connect_deadline && time >= connect_deadline) {
      esp_wifi_disconnect(); policy.failed(time); submitting.store(false); ++view.status.errors; publish();
    }
    if (ready && policy.due(time)) { connect(); publish(); }
    if (ready && close_portal && time >= close_portal && policy.stored) {
      if (esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK) {
        web_stop();
        dns_stop();
        policy.portal = false; local.active = false; wipe(local.key, sizeof local.key); wipe(view.token, sizeof view.token);
      } else { ++view.status.errors; close_portal = time + 2000; }
      if (!policy.portal) close_portal = 0;
      publish();
    }
    if (ready && scanning.load() && scan_deadline && time >= scan_deadline) {
      esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); scanning.store(false);
      scan_deadline = 0; ++view.status.errors; publish();
    }
    // Reconcile dropped driver events against actual link/IP, never guessed success.
    if (ready && (rejected.load() != previous_rejected ||
        (policy.state == State::CONNECTING && !received))) {
      previous_rejected = rejected.load(); got_ip();
      if (policy.state == State::CONNECTED && !healthy()) policy.failed(time);
      publish();
    }
    diagnostics::Address desired{};
    esp_netif_ip_info_t dip{};
    if(ready && policy.state==State::CONNECTED && !policy.portal && !web_stats().http && !dns_active() &&
       healthy() && sta && esp_netif_get_ip_info(sta,&dip)==ESP_OK) {
      desired={ntohl(dip.ip.addr),ntohl(dip.netmask.addr),policy.generation,true};
    }
    diagnostics::service(desired);
    if(time-diagnostic_time>=1000 || diagnostic_state!=unsigned(policy.state) || diagnostic_errors!=view.status.errors) {
      // Includes IP changes reconciled by healthy(), even without a queued event.
      if (recovery.ticket && (recovery.epoch!=policy.generation || time>recovery.deadline)) recovery.invalidate();
      publish();
      diagnostics::Network n{}; n.time=time;
      n.values={unsigned(policy.state),policy.state==State::CONNECTED,policy.stored,view.status.storage_ready,policy.portal,policy.generation,policy.reconnects,view.status.reason,view.status.scan_count,view.status.errors,view.status.allocation_errors,rejected.load(),uxTaskGetStackHighWaterMark(nullptr),web_stack_min(),diagnostics::errors(),diagnostics::stack_min()};
      n.rssi=std::int8_t(view.status.rssi);n.http_dns_off=!web_stats().http&&!dns_active();std::snprintf(n.ip,sizeof n.ip,"%s",view.status.ip);
      if(policy.state==State::CONNECTED) std::snprintf(n.ssid,sizeof n.ssid,"%s",credentials.ssid);
      diagnostics::publish(n);
      if(diagnostic_state!=unsigned(policy.state))diagnostics::event(time,diagnostics::Event::NETWORK,unsigned(policy.state));
      if(diagnostic_errors!=view.status.errors)diagnostics::event(time,diagnostics::Event::NETWORK_ERROR,view.status.errors);
      diagnostic_time=time;diagnostic_state=unsigned(policy.state);diagnostic_errors=view.status.errors;
    }
    if (!received) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(dns_active()?100:1000));
  }
}
}
bool web_view(WebView &out) {
  if (!shared_mutex || xSemaphoreTake(shared_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return false;
  out = published; xSemaphoreGive(shared_mutex); return true;
}
bool read_status(Status &out) {
  if (!shared_mutex || xSemaphoreTake(shared_mutex, 0) != pdTRUE) return false;
  out = published.status; xSemaphoreGive(shared_mutex); return true;
}
bool read_settings(SettingsView &out) {
  if (!shared_mutex || xSemaphoreTake(shared_mutex, 0) != pdTRUE) return false;
  out = settings; xSemaphoreGive(shared_mutex); return true;
}
bool settings_request(SettingsAction action, unsigned epoch, std::uint64_t confirmation) {
  if (sleep_channel.busy() || unsigned(action)>unsigned(SettingsAction::FACTORY_RESET) || action==SettingsAction::FACTORY_RESET || !owner.load()) return false;
  Message m{}; m.kind=Kind::SETTINGS; m.generation=epoch; m.detail=unsigned(action); m.confirmation=confirmation;
  return post(m);
}
bool local_setup(LocalSetup &out) {
  if (!shared_mutex || xSemaphoreTake(shared_mutex, 0) != pdTRUE) return false;
  out = setup; xSemaphoreGive(shared_mutex); return true;
}
bool submit(const Credentials &c, unsigned epoch) {
  if(sleep_channel.busy())return false;
  WebView v{};
  if (!web_view(v) || !v.status.portal || v.status.state != State::PROVISIONING ||
      v.status.generation != epoch || scanning.load()) return false;
  bool free = false;
  if (!valid(c) || !submitting.compare_exchange_strong(free, true)) return false;
  Message m{}; m.kind = Kind::SUBMIT; m.credentials = c; m.generation = epoch;
  bool ok = post(m); wipe(&m, sizeof m); if (!ok) submitting.store(false); return ok;
}
bool scan(unsigned epoch) {
  if(sleep_channel.busy())return false;
  WebView v{};
  if (!web_view(v) || !v.status.portal || v.status.state != State::PROVISIONING ||
      v.status.generation != epoch || submitting.load()) return false;
  bool free = false;
  if (!scanning.compare_exchange_strong(free, true)) return false;
  Message m{}; m.kind = Kind::SCAN; m.generation = epoch;
  bool ok = post(m); if (!ok) scanning.store(false); return ok;
}
bool development_key(unsigned char c) { return c == 'W' || c == 'G' || c == 'Y' || c == 'R'; }
bool development_command(unsigned char c) {
  if (sleep_channel.busy() || !development_key(c) || (c=='R' && !owner.load())) return false;
  Message m{}; m.kind = c == 'R' ? Kind::REBOOT : c == 'W' ? Kind::STATUS : c == 'G' ? Kind::ARM_CLEAR : Kind::CONFIRM_CLEAR; return post(m);
}
unsigned sleep_prepare(){
  if(!owner.load())return 0;
  const auto epoch=sleep_channel.begin();
  if(epoch)xTaskNotifyGive(owner.load());
  return epoch;
}
bool sleep_ready(unsigned epoch){return sleep_channel.ready(epoch);}
sleep::Reply sleep_reply(unsigned epoch){return sleep_channel.reply(epoch);}
void sleep_resume(unsigned epoch){sleep_channel.resume(epoch);if(auto task=owner.load())xTaskNotifyGive(task);}
void start() {
  if (shared_mutex) return; // Singleton; repeated start must not create another owner.
  shared_mutex = xSemaphoreCreateMutex();
  if (!shared_mutex || xTaskCreatePinnedToCore(task, "NETWORK", 6144, nullptr, 1, nullptr, 0) != pdPASS)
    ESP_LOGE("NETWORK", "init=FAILED local_UI_unaffected=1");
}
void fault(bool allocation) { Message m{}; m.kind = Kind::FAULT; m.detail = allocation; post(m); }
}
