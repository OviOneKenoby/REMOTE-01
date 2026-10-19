idf_component_register(
    SRCS "core.cpp" "ingress.cpp" "mock_adapter.cpp" "memory_metrics.cpp"
    INCLUDE_DIRS "include"
    WHOLE_ARCHIVE)
# Retain the detached foundation in the firmware for honest link-size measurement.
# No runtime instance, UI hook or RTOS task is introduced.
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror
    -fno-function-sections -fno-data-sections)
# Whole-archive extracts objects but section GC can still drop an unused module.
# Retain each implementation translation unit, without calling it at runtime.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--undefined=remote01_control_core_anchor"
    "-Wl,--undefined=remote01_control_mock_anchor"
    "-Wl,--undefined=remote01_control_sizes")
