idf_component_register(SRCS "association.cpp" "wire.cpp" "client.cpp" "mdns_discovery.cpp"
    INCLUDE_DIRS "include"
    REQUIRES remote_persistence network_manager nvs_flash
    PRIV_REQUIRES lwip mbedtls esp_system mdns)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
