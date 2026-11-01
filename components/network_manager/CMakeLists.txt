idf_component_register(SRCS "model.cpp" "presentation.cpp" "portal.cpp" "dns.cpp" "runtime.cpp" "web.cpp" "ui.cpp"
 INCLUDE_DIRS "include"
 PRIV_REQUIRES diagnostics ota_service power_management esp_driver_usb_serial_jtag esp_wifi esp_netif esp_event nvs_flash esp_http_server json esp_timer
 esp_hw_support esp_system heap freertos lwip lvgl)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
