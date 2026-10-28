idf_component_register(SRCS "main.c" "ptt_button.c" "ptt_hid.c" "ptt_config_check.cpp"
                            "ptt_mode.c" "ptt_storage.c" "ptt_cfgfile.c" "ptt_keynames.c" "ptt_msc.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_driver_gpio esp_hw_support esp_partition esp_app_format
                                     fatfs wear_levelling vfs)
