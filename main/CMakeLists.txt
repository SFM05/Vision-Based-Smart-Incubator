set(srcs "main.cpp"
         "camera/camera_driver.c"
         "lcd/lcd_driver.c"
         "pipeline/video_pipeline.c"
         "yolo26/yolo_inference.cpp"
         "yolo26/image.cpp")
set(requires esp_lcd
             driver
             esp_video
             esp_lcd_ek79007
             freertos
             esp-dl
             esp_partition)
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
esptool_py_flash_to_partition(flash "image" "${img}")
