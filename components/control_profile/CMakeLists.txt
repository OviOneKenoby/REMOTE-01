idf_component_register(SRCS "profile.cpp" "runtime.cpp"
    INCLUDE_DIRS "include"
    REQUIRES control
    PRIV_REQUIRES esp_timer esp_system esp_hw_support heap freertos)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -fstack-usage)
