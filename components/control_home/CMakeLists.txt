idf_component_register(SRCS "facade.cpp" "development.cpp" "rtcp_device.cpp" "runtime.cpp"
    INCLUDE_DIRS "include" REQUIRES control
    PRIV_REQUIRES diagnostics network_manager power_management remote_persistence rtcp_client esp_timer esp_system esp_hw_support heap freertos driver esp_driver_usb_serial_jtag)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
