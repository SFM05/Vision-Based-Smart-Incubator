idf_build_get_property(component_targets __COMPONENT_TARGETS)
if ("___idf_espressif__esp-dl" IN_LIST component_targets)
   idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
elseif("___idf_esp-dl" IN_LIST component_targets)
   idf_component_get_property(espdl_dir esp-dl COMPONENT_DIR)
endif()
if ("___idf_espressif__esp_video" IN_LIST component_targets)
   idf_component_get_property(esp_video_dir espressif__esp_video COMPONENT_DIR)
endif()
if ("___idf_espressif__esp_sccb_intf" IN_LIST component_targets)
   idf_component_get_property(esp_sccb_dir espressif__esp_sccb_intf COMPONENT_DIR)
endif()
if ("___idf_espressif__esp_cam_sensor" IN_LIST component_targets)
   idf_component_get_property(esp_cam_sensor_dir espressif__esp_cam_sensor COMPONENT_DIR)
endif()
set(cmake_dir ${espdl_dir}/fbs_loader/cmake)
include(${cmake_dir}/utilities.cmake)

set(srcs "main.cpp"
         "lcd/st7796_driver.cpp"
         "lcd/ft6336_driver.cpp"
         "inference_task/inference_task.cpp"
         "lvgl_port/lvgl_port.cpp"
         "yolo26/yolo_inference.cpp"
         "yolo26/image_auto.cpp"
         "ui/gui_guider.c"
         "ui/setup_scr_screen_1.c"
         "ui/setup_scr_screen_2.c"
         "ui/events_init.c")
set(requires esp_lcd
             driver
             esp_video
             esp_driver_spi
             esp_driver_i2c
             esp_lcd_st7796
             esp_lcd_touch_ft5x06
             espressif__esp_sccb_intf
             espressif__esp_cam_sensor
             freertos
             lvgl
             esp-dl
             esp_partition
             yolo26)
set(include_dirs "."
                 "lcd"
                 "lvgl_port"
                 "ui"
                 "inference_task"
                 "yolo26"
                 "${esp_video_dir}/include"
                 "${esp_video_dir}/private_include"
                 "${esp_sccb_dir}/include"
                 "${esp_sccb_dir}/sccb_i2c/include"
                 "${esp_cam_sensor_dir}/include"
                 "${esp_cam_sensor_dir}/sensors/ov5647/include")

idf_component_register(SRCS ${srcs}
                      REQUIRES ${requires}
                      INCLUDE_DIRS ${include_dirs})

set(model ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/yolo26n_640.espdl)
esptool_py_flash_to_partition(flash "yolo26" "${model}")

set(img ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/12995.jpg)
file(SIZE "${img}" IMG_FILE_SIZE)
target_compile_definitions(${COMPONENT_LIB} PRIVATE IMAGE_SIZE=${IMG_FILE_SIZE})
esptool_py_flash_to_partition(flash "image" "${img}")
