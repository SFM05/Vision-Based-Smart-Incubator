set(srcs "main.cpp"
         "camera/camera_driver.c"
         # "lcd/lcd_driver.c"
         "lcd/st7796_driver.cpp"
         "lcd/ft6336_driver.cpp"
         "pipeline/video_pipeline.c"
         "yolo26/yolo_inference.cpp"
         "yolo26/image_auto.cpp")
set(requires esp_lcd
             driver
             esp_video
            #  esp_lcd_ek79007
             esp_driver_spi
             esp_driver_i2c
             unity
             esp_lcd_st7796
             esp_lcd_touch_ft5x06
             freertos
             esp-dl
             esp_partition
             yolo26)
set(include_dirs "."
                 "camera"
                 "lcd"
                 "pipeline"
                 "yolo26")

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


# This is added after the registery for esp-dl
set(model ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/yolo26n_640.espdl)
esptool_py_flash_to_partition(flash "yolo26" "${model}")

set(img ${CMAKE_CURRENT_SOURCE_DIR}/yolo26/12995.jpg)
file(SIZE "${img}" IMG_FILE_SIZE)
target_compile_definitions(${COMPONENT_LIB} PRIVATE IMAGE_SIZE=${IMG_FILE_SIZE})
esptool_py_flash_to_partition(flash "image" "${img}")
