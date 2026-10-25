idf_component_register(SRCS "main.c" "ptt_button.c" "ptt_hid.c" "ptt_config_check.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_hw_support)
