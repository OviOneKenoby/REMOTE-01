idf_component_register(
    SRCS "backend.cpp" "esp_nvs_store.cpp" "diagnostic.cpp" "diagnostic_runtime.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash esp_partition
    WHOLE_ARCHIVE)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror
    -fno-function-sections -fno-data-sections)
# Retain both dormant implementation units in the candidate ELF. These anchors
# are linker-only and are never called, so they cannot open or write NVS.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--undefined=remote_persistence_backend_anchor"
    "-Wl,--undefined=remote_persistence_esp_nvs_anchor")
