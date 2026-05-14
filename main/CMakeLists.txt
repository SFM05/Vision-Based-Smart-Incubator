idf_component_register(SRCS "main.cpp" "st7796.cpp" "ft6336.cpp"
                           "lv_port_disp.cpp" "lv_port_indev.cpp"
                           "camera.cpp"
                     INCLUDE_DIRS "."
                    REQUIRES driver esp_driver_cam esp_driver_isp
                              esp_driver_i2c esp_hw_support lvgl esp_timer sensor_init)
