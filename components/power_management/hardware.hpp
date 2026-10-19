#pragma once
#include "esp_err.h"
#include "power_management/qmi_wom.hpp"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

namespace power_management {
struct Board {
  adc_oneshot_unit_handle_t adc = nullptr;
  adc_cali_handle_t calibration = nullptr;
  bool adc_ready = false, imu_ready = false;
  adc_cali_handle_t external_calibration = nullptr;
  esp_err_t external_error = ESP_ERR_INVALID_STATE;
  bool external_ready = false;
  bool read(unsigned reg, unsigned char &value);
  bool write(unsigned reg, unsigned char value);
  unsigned now();
  void delay(unsigned ms);
#if POWER_WOM_DIAGNOSTIC
  WomDiagnostic &wom_diagnostic(); // POWER-owned boot-only static record, no Board growth.
#endif
  bool initialize();
  bool battery(unsigned &raw, unsigned &mv);
  __attribute__((noinline)) void initialize_external();
  __attribute__((noinline)) esp_err_t external(unsigned &raw, unsigned &mv);
  bool input(unsigned &bits);
  bool display(bool enable);
  bool release_hold();
};
} // namespace power_management
