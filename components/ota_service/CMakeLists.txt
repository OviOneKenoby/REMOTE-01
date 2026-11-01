idf_component_register(
  SRCS "model.cpp" "service.cpp"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES esp_http_server app_update esp_partition
                esp_app_format esp_timer esp_system heap freertos mbedtls
                )
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
