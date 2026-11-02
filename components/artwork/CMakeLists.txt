idf_component_register(
  SRCS "surface.cpp" "online_policy.cpp" "musicbrainz.cpp" "online_runtime.cpp" "tls_record_buffer_psram.cpp"
  REQUIRES lvgl
  PRIV_REQUIRES heap esp_http_client mbedtls esp_timer esp_app_format freertos network_manager ota_service power_management
  INCLUDE_DIRS "include")
