idf_component_register(SRCS "model.cpp" "service.cpp" INCLUDE_DIRS "include"
 PRIV_REQUIRES ota_service esp_http_server esp_netif esp_wifi esp_timer esp_system esp_app_format heap freertos lwip)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
