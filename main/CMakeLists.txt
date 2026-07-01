set(srcs "main.cpp"
         "lcd/st7796_driver.cpp"
         "lcd/ft6336_driver.cpp"
         "sensors/sensor.c"
         "sensors/sensor_task.cpp"
         "inference_task/inference_task.cpp"
         "lvgl_port/lvgl_port.cpp"
         "yolo26/yolo_inference.cpp"
         "yolo26/image_auto.cpp"
         "ui/gui_guider.c"
         "ui/setup_scr_screen_1.c"
         "ui/setup_scr_screen_2.c"
         "ui/events_init.c"
         "iot/mqtt.cpp"
         "iot/iot_utils.cpp"
         "iot/private/utils.c"
         "iot/eth.cpp"
         "sdcard/sdcard.cpp"
         "test.cpp"
         "process.cpp")
set(requires esp_lcd
             driver
             esp_video
             esp_driver_spi
             esp_driver_i2c
             esp_lcd_st7796
             esp_lcd_touch_ft5x06
             espressif__esp_sccb_intf
             espressif__esp_cam_sensor
             lvgl
             fatfs
             esp-dl
             esp_partition
             yolo26
             lwip
             mbedtls
             nvs_flash
             mqtt
             esp_http_client
             cjson
             esp_netif
             esp_eth
             ip101)
set(include_dirs "."
                 "lcd"
                 "lvgl_port"
                 "ui"
                 "inference_task"
                 "yolo26"
                 "sensors"
                 "iot"
                 "sdcard")

# This is added before the registery for esp-dl
idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
   idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
   idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()

set(cmake_dir ${espdl_dir}/fbs_loader/cmake)
include(${cmake_dir}/utilities.cmake)

idf_component_register(SRCS ${srcs}
                      REQUIRES ${requires}
                      INCLUDE_DIRS ${include_dirs})

set(model ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/yolo26n_640.espdl)
esptool_py_flash_to_partition(flash "yolo26" "${model}")

set(img ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/12995.jpg)
file(SIZE "${img}" IMG_FILE_SIZE)
target_compile_definitions(${COMPONENT_LIB} PRIVATE IMAGE_SIZE=${IMG_FILE_SIZE})
esptool_py_flash_to_partition(flash "image" "${img}")
