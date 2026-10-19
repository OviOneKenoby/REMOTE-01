idf_component_register(
  SRCS "user_app.cpp"
  PRIV_REQUIRES lvgl ui_bsp lcd_bl_pwm_bsp control_home power_management network_manager physical_input esp_driver_gpio esp_timer
  INCLUDE_DIRS "./")
