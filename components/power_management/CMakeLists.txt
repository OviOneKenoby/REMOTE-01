idf_component_register(SRCS "model.cpp" "hardware.cpp" "runtime.cpp" "reset_diagnostic.cpp"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES network_manager diagnostics i2c_bsp esp_adc esp_timer freertos esp_driver_gpio
    esp_hw_support esp_system espressif__esp_io_expander_tca9554)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../SensorLib/src")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
