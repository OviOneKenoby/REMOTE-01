# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXCLUDE_COMPONENTS
	adc_bsp
	audio_bsp
	ble_scan_bsp
	button_bsp
	codec_board
	esp_wifi_bsp
	i2c_equipment
	sdcard_bsp
	SensorLib
)
project(12_LVGL_Test)
