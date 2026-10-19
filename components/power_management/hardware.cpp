#include "hardware.hpp"
#include "power_management/reset_diagnostic.hpp"
#include "i2c_bsp.h"
#include "esp_io_expander_tca9554.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/adc_channel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern esp_io_expander_handle_t io_expander;
namespace power_management {
#if POWER_WOM_DIAGNOSTIC
namespace {
WomDiagnostic wom_record; // fixed internal storage; only POWER touches during initialization
__attribute__((noinline)) void report_wom() {
  const auto &d=wom_record;
  ESP_LOGI("POWER_WOM", "DEVELOPMENT_ONLY reset=%u ok=%u stage=%u failed=%u op=%u reg=0x%02x err=%ld expected=0x%02x mask=0x%02x actual=0x%02x actual_valid=%u polls=%u phase_polls=%u elapsed_ms=%u total_ms=%u cause=%u",
    unsigned(esp_reset_reason()),unsigned(d.result==2),unsigned(d.stage),unsigned(d.failed),unsigned(d.op),unsigned(d.reg),
    static_cast<long>(d.error),unsigned(d.expected),unsigned(d.mask),unsigned(d.actual),unsigned(d.actual_valid),d.polls,d.stage_polls,d.elapsed_ms,d.total_ms,d.cause);
}
}
WomDiagnostic &Board::wom_diagnostic() {return wom_record;}
#endif
bool Board::read(unsigned reg, unsigned char &v) {
  unsigned char r = reg;
  const auto error=i2c_master_transmit_receive(imu_dev_handle, &r, 1, &v, 1, 20);
#if POWER_WOM_DIAGNOSTIC
  if(wom_record.result==1) wom_record.error=error;
#endif
  return error==ESP_OK;
}
bool Board::write(unsigned reg, unsigned char v) {
  unsigned char data[]{static_cast<unsigned char>(reg), v};
  const auto error=i2c_master_transmit(imu_dev_handle, data, 2, 20);
#if POWER_WOM_DIAGNOSTIC
  if(wom_record.result==1) wom_record.error=error;
#endif
  return error==ESP_OK;
}
unsigned Board::now() { return esp_timer_get_time() / 1000; }
void Board::delay(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
bool Board::initialize() {
  reset_event(reset_diagnostic::Event::ADC_BEGIN, 1); // setup, not conversion
  adc_oneshot_unit_init_cfg_t u{}; u.unit_id = ADC_UNIT_1;
  adc_oneshot_chan_cfg_t channel{};
  channel.atten = ADC_ATTEN_DB_12; channel.bitwidth = ADC_BITWIDTH_12;
  adc_cali_curve_fitting_config_t c{};
  c.unit_id = ADC_UNIT_1; c.chan = ADC_CHANNEL_3; c.atten = ADC_ATTEN_DB_12; c.bitwidth = ADC_BITWIDTH_12;
  adc_ready = adc_oneshot_new_unit(&u, &adc) == ESP_OK &&
      adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &channel) == ESP_OK &&
      adc_cali_create_scheme_curve_fitting(&c, &calibration) == ESP_OK;
  // No guessed reference-voltage fallback if eFuse calibration is unavailable.
  reset_event(reset_diagnostic::Event::IMU_BEGIN, 1); // initial WoM configuration
  imu_ready = Wom::configure(*this);
#if POWER_WOM_DIAGNOSTIC
  report_wom(); // one summary after configuration; never logs inside transactions/polls
#endif
  initialize_external(); // After unchanged WoM init/summary; separate channel calibration.
  ESP_LOGI("EXTERNAL_POWER", "GPIO1 ADC1_CH0 atten_db=12 bits=12 calibrated=%u init_error=%ld thresholds=PROVISIONAL absent_mv=600 present_mv=1800 confirmations=3 sample_ms=2000 charging=UNKNOWN", external_ready, static_cast<long>(external_error));
  ESP_LOGI("POWER", "ADC calibrated=%u IMU_WOM=%u odr_hz=21 threshold_mg=200 INT1=EXIO2 EXIO_INT=GPIO8", adc_ready, imu_ready);
  return adc_ready && imu_ready;
}
bool Board::battery(unsigned &raw, unsigned &mv) {
  raw = mv = 0;
  if (!adc_ready) return false;
  unsigned total = 0;
  for (unsigned i = 0; i < 4; ++i) {
    int value = 0;
    if (adc_oneshot_read(adc, ADC_CHANNEL_3, &value) != ESP_OK) return false;
    total += value;
  }
  raw = (total + 2) / 4;
  int adc_mv = 0;
  if (adc_cali_raw_to_voltage(calibration, raw, &adc_mv) != ESP_OK || adc_mv <= 0) return false;
  mv = unsigned(adc_mv) * 3; // schematic 200k/100k divider, official x3
  return true;
}
static_assert(ADC1_CHANNEL_0_GPIO_NUM==1 && ADC1_CHANNEL_3_GPIO_NUM==4);
void Board::initialize_external() {
  if(!adc) {external_error=ESP_ERR_INVALID_STATE;return;}
  adc_oneshot_chan_cfg_t channel{};
  channel.atten=ADC_ATTEN_DB_12;channel.bitwidth=ADC_BITWIDTH_12;
  external_error=adc_oneshot_config_channel(adc,ADC_CHANNEL_0,&channel);
  if(external_error!=ESP_OK)return;
  adc_cali_curve_fitting_config_t c{};
  c.unit_id=ADC_UNIT_1;c.chan=ADC_CHANNEL_0;c.atten=ADC_ATTEN_DB_12;c.bitwidth=ADC_BITWIDTH_12;
  external_error=adc_cali_create_scheme_curve_fitting(&c,&external_calibration);
  external_ready=external_error==ESP_OK;
}
esp_err_t Board::external(unsigned &raw,unsigned &mv) {
  raw=mv=0;
  if(!external_ready)return external_error;
  unsigned total=0;
  for(unsigned i=0;i<4;++i) {
    int value=0;const auto error=adc_oneshot_read(adc,ADC_CHANNEL_0,&value);
    if(error!=ESP_OK)return error;
    if(value<0 || value>=4095) {if(value>=0)raw=unsigned(value);return ESP_ERR_INVALID_RESPONSE;}
    total+=unsigned(value);
  }
  raw=(total+2)/4;
  int node_mv=0;const auto error=adc_cali_raw_to_voltage(external_calibration,raw,&node_mv);
  if(error!=ESP_OK)return error;
  if(node_mv<0 || node_mv>3100)return ESP_ERR_INVALID_RESPONSE;
  mv=unsigned(node_mv); // GPIO1 node voltage, not inferred 5V/charging voltage.
  return ESP_OK;
}
bool Board::input(unsigned &bits) {
  uint32_t levels = 0;
  if (esp_io_expander_get_level(io_expander, 0x4d, &levels) != ESP_OK) return false;
  bits = levels; return true;
}
bool Board::display(bool on) {
  return esp_io_expander_set_level(io_expander, 1ULL << 1, on ? 1 : 0) == ESP_OK;
}
bool Board::release_hold() {
  return esp_io_expander_set_level(io_expander, 1ULL << 6, 0) == ESP_OK;
}
} // namespace power_management
