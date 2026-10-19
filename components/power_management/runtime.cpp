#include "power_management/stack_measurement.hpp"
#include "diagnostics/api.hpp"
#include "power_management/api.hpp"
#include "power_management/model.hpp"
#include "power_management/output.hpp"
#include "power_management/reset_diagnostic.hpp"
#include "power_management/wake_irq.hpp"
#include "hardware.hpp"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "network_manager/api.hpp"
#include "power_management/sleep_timer.hpp"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/gpio_struct.h"
#include <atomic>

namespace power_management {
namespace {
std::atomic<Request> request{Request::NONE};
std::atomic<bool> core_parked{false}, core_refused{false}, ui_pause{false}, ui_parked{false};
std::atomic<bool> active{true}, consume_contact{false}, settling{false};
std::atomic<unsigned> physical_activity{0}, command{0};
portMUX_TYPE view_mux = portMUX_INITIALIZER_UNLOCKED;
BatteryView view;
ExternalView external_published;
ExternalPower external_measurement; // fixed POWER-owned state; not persistent task scratch
TaskHandle_t worker = nullptr;
TouchGate touch_gate; // only LVGL touch callback accesses this value
Time time_now() { return esp_timer_get_time() / 1000; }
void IRAM_ATTR interrupt(void *) {
  BaseType_t awake = pdFALSE;
  if (worker) vTaskNotifyGiveFromISR(worker, &awake);
  if (awake) portYIELD_FROM_ISR();
}
const char *state_name(State s) {
  switch (s) {
    case State::ACTIVE: return "ACTIVE";
    case State::DISPLAY_IDLE: return "DISPLAY_IDLE";
    case State::SYSTEM_SLEEP: return "SYSTEM_SLEEP";
    case State::CRITICAL_SHUTDOWN: return "CRITICAL_SHUTDOWN";
  }
  return "UNKNOWN";
}
const char *condition_name(Condition c) {
  switch (c) {
    case Condition::NORMAL: return "NORMAL";
    case Condition::LOW: return "LOW";
    case Condition::CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
  }
}
bool wait_flag(const std::atomic<bool> &flag, bool value, unsigned ms) {
  const auto start = time_now();
  while (flag.load() != value && time_now() - start < ms) vTaskDelay(pdMS_TO_TICKS(10));
  return flag.load() == value;
}
struct WakePort {
  esp_err_t error = ESP_OK;
  bool attempted = false;
  bool mask() { return gpio_intr_disable(GPIO_NUM_8) == ESP_OK; }
  bool arm() { return gpio_wakeup_enable(GPIO_NUM_8, GPIO_INTR_LOW_LEVEL) == ESP_OK; }
  bool sleep() { attempted = true; error = esp_light_sleep_start(); return error == ESP_OK; }
  bool disarm() { return gpio_wakeup_disable(GPIO_NUM_8) == ESP_OK; }
  bool edge() { return gpio_set_intr_type(GPIO_NUM_8, GPIO_INTR_NEGEDGE) == ESP_OK; }
  bool unmask() { return gpio_intr_enable(GPIO_NUM_8) == ESP_OK; }
};
#if POWER_STACK_MEASUREMENT
using StackPath = stack_measurement::Path;
stack_measurement::State stack_records; // fixed internal storage, POWER only
__attribute__((noinline)) void stack_begin(StackPath p) {
  stack_records.begin(p, uxTaskGetStackHighWaterMark(nullptr));
}
__attribute__((noinline)) void stack_end(StackPath p) {
  stack_records.end(p, uxTaskGetStackHighWaterMark(nullptr));
}
const char *stack_name(StackPath p) {
  static const char *const names[]{"PROBE","STARTUP","NORMAL","ADC","INPUT",
    "SNAPSHOT","RESET_COPY","PUBLISH","B","DISPLAY","SLEEP","RELEASE",
    "COMMAND","REPORT","ERROR"};
  static_assert(sizeof(names)/sizeof(names[0])==unsigned(StackPath::COUNT));
  return names[unsigned(p)];
}
__attribute__((noinline)) void stack_pair(const char *kind, StackPath p,
                                         const stack_measurement::Pair &v) {
  ESP_LOGI("POWER_STACK", "%s path=%s seq=%lu:%lu before=%lu after=%lu new=%u",
    kind, stack_name(p), (unsigned long)v.begin, (unsigned long)v.end,
    (unsigned long)v.before, (unsigned long)v.after, v.after<v.before);
}
__attribute__((noinline)) void stack_report() {
  stack_begin(StackPath::REPORT);
  ESP_LOGI("POWER_STACK", "DEVELOPMENT_ONLY epoch=%lu baseline=%lu seq=%lu invalid=%lu saturated=%u",
    (unsigned long)stack_records.epoch, (unsigned long)stack_records.baseline,
    (unsigned long)stack_records.sequence, (unsigned long)stack_records.invalid, stack_records.saturated);
  for(unsigned i=0;i<unsigned(StackPath::COUNT);++i) {
    const auto &r=stack_records.records[i];
    ESP_LOGI("POWER_STACK", "path=%s count=%lu drops=%lu min=%lu",
      stack_name(StackPath(i)), (unsigned long)r.count, (unsigned long)r.drops,
      (unsigned long)(r.count?r.lowest.after:0));
    if(r.count) {
      if(r.last.end) stack_pair("last",StackPath(i),r.last);
      stack_pair("lowest",StackPath(i),r.lowest);
    }
  }
  ESP_LOGI("POWER_STACK", "drop_history=%lu overwritten=%lu report_result=NEXT_S historical_HWM_not_reset=1",
    (unsigned long)stack_records.history_count, (unsigned long)stack_records.overwritten);
  const auto count=stack_records.history_count<16?stack_records.history_count:16;
  for(unsigned i=0;i<count;++i) {
    const auto &d=stack_records.history[(stack_records.history_count-count+i)%16];
    stack_pair("drop",d.path,d.pair);
  }
  stack_end(StackPath::REPORT); // no output after this checkpoint
}
__attribute__((noinline)) void stack_clear() {
  stack_records.clear(uxTaskGetStackHighWaterMark(nullptr));
  stack_begin(StackPath::REPORT);
  ESP_LOGI("POWER_STACK", "DEVELOPMENT_ONLY CLEAR epoch=%lu baseline=%lu historical_HWM_not_reset=1",
    (unsigned long)stack_records.epoch, (unsigned long)stack_records.baseline);
  stack_end(StackPath::REPORT);
}
#define STACK_BEGIN(p) stack_begin(StackPath::p)
#define STACK_END(p) stack_end(StackPath::p)
#else
#define STACK_BEGIN(p) ((void)0)
#define STACK_END(p) ((void)0)
#endif
// POWER calls synchronously; keep publication scratch out of the task frame.
// References preserve owner-local state without heap allocation or RTC exposure.
__attribute__((noinline)) void publish_power(
    Time now, const Machine &machine, const Board &board,
    const Battery &real, const Battery &simulated, const ExternalView &external, bool hold, bool display, bool injection,
    const unsigned long long &sleep_rounds, const unsigned long long &sleep_errors,
    const unsigned long long &motions, const unsigned long long &bus_errors,
    const unsigned long long &measurement_errors, const unsigned long long &wake_latency,
    unsigned last_sleep_cause, Time &diagnostic_time, unsigned &diagnostic_state,
    unsigned long long &diagnostic_errors) {
  STACK_BEGIN(SNAPSHOT);
  diagnostics::Power d{};d.time=now;
  const auto &b=injection?simulated:real;
  d.values={unsigned(machine.state),hold,display,unsigned(machine.reason),machine.wakes,machine.sleep_requests,sleep_rounds,sleep_errors,now-machine.last_activity,board.imu_ready,motions,bus_errors,uxTaskGetStackHighWaterMark(nullptr),b.raw,b.voltage_mv,b.filtered_mv,b.valid,unsigned(b.condition),injection,measurement_errors,wake_latency,last_sleep_cause,board.adc_ready};
  d.external={external.raw,external.millivolts,unsigned(external.state),external.valid,external.calibrated,external.error,external.read_errors};
  STACK_END(SNAPSHOT); STACK_BEGIN(RESET_COPY);
  const auto h=reset_history_copy();
  STACK_END(RESET_COPY); STACK_BEGIN(SNAPSHOT);
  for(unsigned age=0;age<4;++age) { d.resets[age][0]=h.rows[age].available;for(unsigned i=0;i<15;++i)d.resets[age][i+1]=h.rows[age].values[i]; }
  STACK_END(SNAPSHOT); STACK_BEGIN(PUBLISH);
  diagnostics::publish(d);
  if(diagnostic_state!=unsigned(machine.state))diagnostics::event(now,diagnostics::Event::POWER,unsigned(machine.state));
  if(diagnostic_errors!=sleep_errors+bus_errors+measurement_errors)diagnostics::event(now,diagnostics::Event::POWER_ERROR,unsigned(sleep_errors+bus_errors+measurement_errors));
  diagnostic_time=now;diagnostic_state=unsigned(machine.state);diagnostic_errors=sleep_errors+bus_errors+measurement_errors;
  STACK_END(PUBLISH);
}
__attribute__((noinline)) void sample_external(Board &board,Time now) {
  unsigned raw=0,mv=0;
  const auto error=board.external(raw,mv);
  external_measurement.sample(error,raw,mv,board.external_ready,now);
  portENTER_CRITICAL(&view_mux);
  external_published=external_measurement.view;
  portEXIT_CRITICAL(&view_mux);
}
__attribute__((noinline)) void report_external() {
  const auto &v=external_measurement.view;
  ESP_LOGI("EXTERNAL_POWER", "gpio=1 raw=%u mv=%u valid=%u calibrated=%u error=%ld read_errors=%llu external=%s charging=UNKNOWN thresholds=PROVISIONAL",v.raw,v.millivolts,v.valid,v.calibrated,static_cast<long>(v.error),static_cast<unsigned long long>(v.read_errors),external_name(v.state));
}
void task(void *) {
  STACK_BEGIN(PROBE); STACK_END(PROBE);
  STACK_BEGIN(STARTUP);
  using reset_diagnostic::Event;
  worker = xTaskGetCurrentTaskHandle();
  reset_event(Event::WORKER_INIT);
  Board board; board.initialize();
  gpio_config_t g{}; g.pin_bit_mask = 1ULL << 8; g.mode = GPIO_MODE_INPUT;
  g.pull_up_en = GPIO_PULLUP_ENABLE; g.intr_type = GPIO_INTR_NEGEDGE;
  bool irq_ok = gpio_config(&g) == ESP_OK;
  auto service = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  const int irq_core = service == ESP_OK ? xPortGetCoreID() : -1;
  irq_ok = irq_ok && (service == ESP_OK || service == ESP_ERR_INVALID_STATE) &&
      gpio_isr_handler_add(GPIO_NUM_8, interrupt, nullptr) == ESP_OK;
  bool sleep_hardware = irq_ok && board.imu_ready &&
      esp_sleep_enable_gpio_wakeup() == ESP_OK &&
      esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_ON) == ESP_OK;
  Machine machine(time_now()); Battery real, simulated;
  bool injection = false, display = true, hold = true, sim_reported = false;
  unsigned inject_mv = 3800, seen_activity = physical_activity.load(), input = 0;
  Time sample_time = 0, arm_until = 0, settle_until = 0, wake_started = 0;
  Time input_retry = 0, display_retry = 0;
  bool display_fault = false;
  unsigned long long wake_latency = 0;
  unsigned last_sleep_cause = 0;
  unsigned long long sleep_rounds = 0, sleep_errors = 0, measurement_errors = 0, bus_errors = 0, motions = 0;
  bool gpio_woke = false, forced_sleep = false;
  SleepTimer sleep_timer;
  unsigned network_sleep_epoch=0;
  Condition reported = Condition::UNKNOWN;
  ESP_LOGI("POWER", "STAGE1 DEVELOPMENT_ONLY injection=0 hold=EXIO6_HIGH external=UNKNOWN charging=UNKNOWN SOC=UNAVAILABLE display_idle_ms=30000 automatic_system_sleep=ENABLED external_gate=NONE system_idle_ms=300000 forced_sleep=DEVELOPMENT_ONLY sleep_ready=%u keys=B I Z L C H N K !", sleep_hardware);
  ESP_LOGI("POWER", "awake_irq_type=%u expected_negedge=%u irq_service_core=%d worker_cpu=%d level_wake=SYSTEM_SLEEP", unsigned(GPIO.pin[8].int_type), unsigned(GPIO_INTR_NEGEDGE), irq_core, int(xPortGetCoreID()));
  reset_snapshot(machine, real, hold, sleep_rounds);
  reset_event(Event::RUNTIME_READY);
  STACK_END(STARTUP);
  Time diagnostic_time=0; unsigned diagnostic_state=~0u; unsigned long long diagnostic_errors=0;
  for (;;) {
    STACK_BEGIN(NORMAL);
    const auto now = time_now();
    const auto previous = machine.state;
    bool motion = false;
    const bool irq = gpio_get_level(GPIO_NUM_8) == 0 || gpio_woke;
    STACK_END(NORMAL);
    if ((irq || now - sample_time >= 2000) && now >= input_retry) {
      STACK_BEGIN(INPUT);
      reset_event(Event::INPUT_BEGIN);
      if (!board.input(input)) { ++bus_errors; sleep_hardware = false; input_retry = now + 2000; reset_event(Event::INPUT_FAULT); }
      else {
        hold = (input & 0x40) != 0;
        reset_snapshot(machine, real, hold, sleep_rounds); reset_event(Event::INPUT_OK);
      }
      if (board.imu_ready) {
        reset_event(Event::IMU_BEGIN);
        if (!Wom::status(board, motion)) {
          ++bus_errors; board.imu_ready = false; sleep_hardware = false; reset_event(Event::IMU_FAULT);
        } else reset_event(Event::IMU_OK, motion);
      }
      STACK_END(INPUT);
    }
    STACK_BEGIN(DISPLAY);
    if (motion) {
      ++motions;
      if (machine.state != State::ACTIVE) { consume_contact.store(true); settle_until = now + 200; }
      machine.wake(now, Wake::MOTION);
    } else if (gpio_woke) {
      machine.wake(now, Wake::TOUCH); consume_contact.store(true); settle_until = now + 200;
    }
    gpio_woke = false;
    const auto activity = physical_activity.load();
    if (activity != seen_activity) {
      seen_activity = activity;
      if (machine.state != State::ACTIVE) { machine.wake(now, Wake::TOUCH); settle_until = now + 200; }
      else machine.activity(now);
    }
    if (previous != State::ACTIVE && machine.state == State::ACTIVE) wake_started = now;
    STACK_END(DISPLAY);
    if (now - sample_time >= 2000) {
      STACK_BEGIN(ADC);
      unsigned raw = 0, mv = 0;
      reset_event(Event::ADC_BEGIN);
      bool ok = board.battery(raw, mv);
      real.sample(ok, raw, mv, now); sample_time = now;
      sample_external(board,now);
      reset_snapshot(machine, real, hold, sleep_rounds);
      reset_event(real.valid ? Event::ADC_OK : Event::ADC_FAULT);
      if (!real.valid) ++measurement_errors;
      if (injection) simulated.sample(true, 0, inject_mv, now);
      if (injection && simulated.shutdown_required() && !sim_reported) {
        sim_reported = true;
        STACK_BEGIN(ERROR);
        ESP_LOGW("POWER", "CRITICAL_SHUTDOWN decision=1 source=SIMULATED dry_run=1 hold_release=0");
        STACK_END(ERROR);
      }
      if (!injection || simulated.condition != Condition::CRITICAL) sim_reported = false;
      const Battery &shown = injection ? simulated : real;
      portENTER_CRITICAL(&view_mux);
      view = {shown.valid, injection, shown.filtered_mv};
      portEXIT_CRITICAL(&view_mux);
      if (real.condition != reported) {
        reported = real.condition;
        STACK_BEGIN(ERROR);
        ESP_LOGW("POWER", "battery=%s automatic_shutdown=INHIBITED reason=POLICY_UNCHANGED hold_release=0", condition_name(reported));
        STACK_END(ERROR);
      }
      STACK_END(ADC);
    }
    const auto key = command.exchange(0);
#if POWER_STACK_MEASUREMENT
    if (key == 'S') stack_report();
    if (key == 'T') stack_clear();
#endif
#if POWER_STACK_MEASUREMENT
    if (key && !stack_measurement::key(static_cast<unsigned char>(key))) {
#else
    if (key) {
#endif
      STACK_BEGIN(COMMAND);
      reset_event(Event::COMMAND, key);
      ESP_LOGI("POWER_DEV", "key=%c DEVELOPMENT_ONLY source=%s", char(key), injection ? "SIMULATED" : "REAL");
      switch (key) {
        case 'I': machine.state = State::DISPLAY_IDLE; machine.activity(now); break;
        case 'Z': machine.last_activity = now >= Machine::system_idle_ms ? now - Machine::system_idle_ms : 0;
          // Explicit DEVELOPMENT_ONLY override; never normal idle policy.
          if (sleep_hardware && real.valid && hold) { machine.state = State::SYSTEM_SLEEP; forced_sleep=true; ++machine.sleep_requests; }
          else ESP_LOGW("POWER", "forced_sleep=REFUSED wake_or_measurement_prerequisite=0");
          break;
        case 'L': case 'C': case 'H':
          if (!injection) { simulated = Battery{}; simulated.external = External::ABSENT; }
          injection = true; inject_mv = key == 'L' ? 3550 : key == 'C' ? 3200 : 3800;
          ESP_LOGI("BATTERY", "source=SIMULATED input_mv=%u dry_run=1 hold_release=0", inject_mv); break;
        case 'N': injection = false; simulated = Battery{}; sim_reported = false; break;
        case 'K': arm_until = now + 10000; ESP_LOGW("POWER_DEV", "HARD_RELEASE_ARMED expires_ms=10000 next_key=! owner_explicit_test_only=1"); break;
        case '!':
          if (arm_until && now <= arm_until) machine.state = State::CRITICAL_SHUTDOWN;
          else ESP_LOGW("POWER_DEV", "HARD_RELEASE=REFUSED arm_missing_or_expired=1");
          arm_until = 0; break;
        default: break;
      }
      STACK_END(COMMAND);
    }
    STACK_BEGIN(DISPLAY);
    machine.tick(now, sleep_hardware && real.valid && hold, real.external);
    if (machine.state != State::ACTIVE) active.store(false);
    if (machine.state == State::SYSTEM_SLEEP && request.load() == Request::NONE) {
      STACK_END(DISPLAY); STACK_BEGIN(SLEEP);
      if(physical_activity.load()!=seen_activity) {
        machine.wake(time_now(),Wake::TOUCH);consume_contact.store(true);
      } else {
        // Prove that Core can park before stopping Wi-Fi. A pending RTCP
        // request is a normal bounded refusal and must leave the live network
        // and healthy transport untouched.
        core_refused.store(false); request.store(Request::SLEEP);
        if (!wait_flag(core_parked, true, 500) || core_refused.load()) {
          request.store(Request::NONE); machine.refuse_sleep(now);
        } else {
          network_sleep_epoch=network_manager::sleep_prepare();
          const auto network_deadline=time_now()+10000;
          while(network_sleep_epoch && !network_manager::sleep_ready(network_sleep_epoch) && time_now()<network_deadline &&
                network_manager::sleep_reply(network_sleep_epoch)!=network_manager::sleep::Reply::REFUSED &&
                network_manager::sleep_reply(network_sleep_epoch)!=network_manager::sleep::Reply::FAULT) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
          if(!network_manager::sleep_ready(network_sleep_epoch)) {
            ESP_LOGW("POWER","sleep=REFUSED NETWORK_prepare_timeout_or_refusal=1 hold_retained=1");
            network_manager::sleep_resume(network_sleep_epoch);network_sleep_epoch=0;forced_sleep=false;
            machine.refuse_sleep(time_now());
          } else if(physical_activity.load()!=seen_activity) {
            machine.wake(time_now(),Wake::TOUCH);consume_contact.store(true);
          } else {
            ui_pause.store(true);
            if (!wait_flag(ui_parked, true, 500)) {
              ESP_LOGW("POWER", "sleep=REFUSED LVGL_park_timeout=1 hold_retained=1");
              machine.refuse_sleep(now);
            }
          }
        }
      }
      STACK_END(SLEEP); STACK_BEGIN(DISPLAY);
    }
    // Wake/failure resumes Core and publishes its current snapshot before LVGL.
    if (machine.state != State::SYSTEM_SLEEP && request.load() == Request::SLEEP) {
      request.store(Request::NONE);
    }
    if (machine.state != State::SYSTEM_SLEEP && request.load() == Request::NONE && ui_pause.load()) {
      // Keep LVGL and controls parked until the owner has published fresh state.
      if (wait_flag(core_parked, false, 1500)) {
        ui_pause.store(false); wait_flag(ui_parked, false, 500);
      } else active.store(false);
    }
    if(machine.state!=State::SYSTEM_SLEEP && network_sleep_epoch && !ui_pause.load()) {
      network_manager::sleep_resume(network_sleep_epoch);network_sleep_epoch=0;forced_sleep=false;
    }
    bool wanted_display = machine.state == State::ACTIVE;
    if (now >= display_retry) {
      const bool display_changed = display != wanted_display;
      if (display_changed) reset_event(Event::DISPLAY_BEGIN, wanted_display);
      if (!set_display(board, display, wanted_display)) {
        reset_event(Event::DISPLAY_FAULT);
        if (!display_fault) ESP_LOGE("POWER", "display_gate=FAILED sleep_disabled=1 hold_retained=1");
        display_fault = true; display_retry = now + 2000;
        ++bus_errors; sleep_hardware = false; machine.wake(now, Wake::FAULT);
      } else {
        display_fault = false;
        if (display_changed) reset_event(Event::DISPLAY_OK, wanted_display);
      }
    }
    if (machine.state == State::CRITICAL_SHUTDOWN) {
      STACK_END(DISPLAY); STACK_BEGIN(RELEASE);
      reset_snapshot(machine, real, hold, sleep_rounds);
      reset_event(Event::RELEASE_BEGIN);
      request.store(Request::SHUTDOWN);
      if (wait_flag(core_parked, true, 1000)) {
        ui_pause.store(true);
        if (wait_flag(ui_parked, true, 500) && controlled_release(board, core_parked.load(), ui_parked.load(), display, hold)) {
          ESP_LOGW("POWER", "HARD_RELEASE=APPLIED hold=0 terminal=1 USB_may_keep_board_powered=1");
          reset_snapshot(machine, real, hold, sleep_rounds); reset_event(Event::RELEASE_OK);
          STACK_END(RELEASE);
          for (;;) vTaskDelay(pdMS_TO_TICKS(1000)); // never reassert/reboot on low battery
        }
      }
      ESP_LOGE("POWER", "HARD_RELEASE=REFUSED quiesce_or_expander_failure=1 hold_retained=1");
      reset_event(Event::RELEASE_REFUSED);
      request.store(Request::NONE); machine.wake(now, Wake::FAULT);
      consume_contact.store(true); settle_until = time_now() + 200;
      STACK_END(RELEASE); STACK_BEGIN(DISPLAY);
    }
    if (previous != State::ACTIVE && machine.state == State::ACTIVE) {
      consume_contact.store(true); settle_until = time_now() + 200; wake_started = now;
    }
    settling.store(now < settle_until);
    active.store(machine.state == State::ACTIVE && display && request.load() == Request::NONE && !core_parked.load() && !ui_pause.load() && !ui_parked.load());
    if (wake_started && active.load() && !settling.load()) {
      wake_latency = time_now() - wake_started; wake_started = 0;
      ESP_LOGI("POWER", "manager_wake_to_ready_ms=%llu", wake_latency);
    }
    if (machine.state != previous) {
      reset_snapshot(machine, real, hold, sleep_rounds); reset_event(Event::TRANSITION, unsigned(machine.reason));
      ESP_LOGI("POWER", "state=%s wake=%u hold=%u display=%u", state_name(machine.state), unsigned(machine.reason), hold, display);
    }
    STACK_END(DISPLAY);
    if (key == 'B') {
      STACK_BEGIN(B);
      reset_report();
      report_external();
      ESP_LOGI("POWER", "awake_irq_type=%u expected_negedge=%u gpio_low=%u irq_service_core=%d worker_cpu=%d", unsigned(GPIO.pin[8].int_type), unsigned(GPIO_INTR_NEGEDGE), gpio_get_level(GPIO_NUM_8) == 0, irq_core, int(xPortGetCoreID()));
      ESP_LOGI("POWER", "automatic_system_sleep=ENABLED external_gate=NONE system_idle_ms=300000 forced_sleep=DEVELOPMENT_ONLY");
      ESP_LOGI("POWER", "state=%s hold=%u wake=%u wakes=%llu sleep_requests=%llu rounds=%llu errors=%llu idle_ms=%llu IMU=%u motions=%llu bus_errors=%llu", state_name(machine.state), hold, unsigned(machine.reason), static_cast<unsigned long long>(machine.wakes), static_cast<unsigned long long>(machine.sleep_requests), sleep_rounds, sleep_errors, static_cast<unsigned long long>(now - machine.last_activity), board.imu_ready, motions, bus_errors);
      ESP_LOGI("BATTERY", "source=REAL calibrated=%u raw=%u mv=%u filtered_mv=%u valid=%u condition=%s external=%s charging=UNKNOWN SOC=UNAVAILABLE read_errors=%llu injection=%u simulated_mv=%u simulated_condition=%s dry_run=1", board.adc_ready, real.raw, real.voltage_mv, real.filtered_mv, real.valid, condition_name(real.condition), external_name(external_measurement.view.state), measurement_errors, injection, simulated.filtered_mv, condition_name(simulated.condition));
      ESP_LOGI("POWER", "stack_min_bytes=%u INT_free=%u PSRAM_free=%u", unsigned(uxTaskGetStackHighWaterMark(nullptr)), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
      ESP_LOGI("POWER", "last_sleep_cause=%u manager_wake_to_ready_ms=%llu", last_sleep_cause, wake_latency);
      STACK_END(B);
    }
    if(now-diagnostic_time>=1000 || diagnostic_state!=unsigned(machine.state) || diagnostic_errors!=sleep_errors+bus_errors+measurement_errors) {
      publish_power(now, machine, board, real, simulated, external_measurement.view, hold, display, injection,
        sleep_rounds, sleep_errors, motions, bus_errors, measurement_errors, wake_latency,
        last_sleep_cause, diagnostic_time, diagnostic_state, diagnostic_errors);
    }
    if (machine.state == State::SYSTEM_SLEEP && core_parked.load() && ui_parked.load()) {
      STACK_BEGIN(SLEEP);
      // A source/prerequisite failure while parked must never strand the user.
      if (!sleep_hardware || !real.valid || !hold) { machine.wake(now, Wake::FAULT); STACK_END(SLEEP); continue; }
      // STATUS1 acknowledgement changes INT1; refresh TCA input comparison
      // before sleeping. A stuck/shared interrupt never creates a busy loop.
      if (!board.input(input) || gpio_get_level(GPIO_NUM_8) == 0) {
        ++bus_errors; machine.wake(now, Wake::FAULT); STACK_END(SLEEP); continue;
      }
      reset_snapshot(machine, real, hold, sleep_rounds); reset_event(Event::SLEEP_BEGIN);
      // Normal physical-only wake; development Z retains original health fallback.
      const bool timer_ok=sleep_timer.configure(forced_sleep,
        []{return esp_sleep_enable_timer_wakeup(2000000)==ESP_OK;},
        []{return esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER)==ESP_OK;});
      if(physical_activity.load()!=seen_activity) {
        machine.wake(time_now(),Wake::TOUCH);consume_contact.store(true);STACK_END(SLEEP);continue;
      }
      if(!timer_ok || !network_manager::sleep_ready(network_sleep_epoch)) {
        ++sleep_errors;machine.refuse_sleep(time_now());STACK_END(SLEEP);continue;
      }
      WakePort wake_port;
      const auto cycle = level_wake_cycle(wake_port);
      auto err = cycle == WakeCycle::OK ? ESP_OK : (wake_port.error != ESP_OK ? wake_port.error : ESP_FAIL);
      if (wake_port.attempted) ++sleep_rounds;
      if (cycle == WakeCycle::RESTORE_FAILED) {
        sleep_hardware = false; // IRQ stays masked; existing task polling remains.
        ESP_LOGE("POWER", "wake_irq_restore=FAILED sleep_disabled=1 polling_retained=1");
      }
      reset_snapshot(machine, real, hold, sleep_rounds); reset_event(Event::SLEEP_END, unsigned(err));
      if (err != ESP_OK) { ++sleep_errors; machine.refuse_sleep(time_now()); }
      else {
        last_sleep_cause = unsigned(esp_sleep_get_wakeup_cause());
        gpio_woke = last_sleep_cause == unsigned(ESP_SLEEP_WAKEUP_GPIO);
      }
      STACK_END(SLEEP);
    } else { STACK_BEGIN(NORMAL); ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)); STACK_END(NORMAL); }
  }
}
} // namespace
bool touch(bool pressed) {
  if (pressed) physical_activity.fetch_add(1);
  return touch_gate.accept(pressed, active.load(), consume_contact.exchange(false), settling.load());
}
bool commands_allowed() { return active.load() && request.load() == Request::NONE; }
BatteryView battery_view() {
  portENTER_CRITICAL(&view_mux); auto copy = view; portEXIT_CRITICAL(&view_mux); return copy;
}
ExternalView external_view() {
  portENTER_CRITICAL(&view_mux);auto copy=external_published;portEXIT_CRITICAL(&view_mux);return copy;
}
bool development_key(unsigned char c) {
#if POWER_STACK_MEASUREMENT
  if (stack_measurement::key(c)) return true;
#endif
  switch (c) { case 'B': case 'I': case 'Z': case 'L': case 'C': case 'H': case 'N': case 'K': case '!': return true; default: return false; }
}
bool development_command(unsigned char c) {
  unsigned empty = 0; return development_key(c) && command.compare_exchange_strong(empty, c);
}
Request owner_request() { return request.load(); }
void owner_ack(bool parked, bool refused) { core_refused.store(refused); core_parked.store(parked); }
bool ui_pause_requested() { return ui_pause.load(); }
void ui_ack(bool parked) { ui_parked.store(parked); }
void start() {
  if (xTaskCreatePinnedToCore(task, "POWER", 4096, nullptr, 2, nullptr, 1) != pdPASS) {
    reset_event(reset_diagnostic::Event::WORKER_FAILED);
    ESP_LOGE("POWER", "worker_init=FAILED idle_sleep_disabled=1 hold_retained=1");
  }
}
} // namespace power_management
