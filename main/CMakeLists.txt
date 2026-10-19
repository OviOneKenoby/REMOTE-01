idf_component_register(
    SRCS "main.cpp"
    PRIV_REQUIRES
        control_profile
        control_home
        remote_persistence
        rtcp_client
        network_manager
        power_management
        user_app
        i2c_bsp
        lvgl
        esp_lcd_axs15231b
        espressif__esp_io_expander_tca9554
        driver
        esp_timer
    INCLUDE_DIRS ".")
