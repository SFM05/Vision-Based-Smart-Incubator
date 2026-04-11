set(srcs "main.cpp"
         "camera/camera_driver.c"
         "lcd/lcd_driver.c"
         "pipeline/video_pipeline.c")
set(requires esp_lcd
             driver
             esp_video
             esp_lcd_ek79007
             freertos
             esp-dl)
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

set(embed_files yolo26/yolo26n_640.espdl
                yolo26/12995.jpg)



idf_component_register(SRCS ${srcs}
                      REQUIRES ${requires}
                      INCLUDE_DIRS ${include_dirs}
                      EMBED_FILES ${embed_files})


# This is added after the registery for esp-dl
target_add_aligned_binary_data(${COMPONENT_LIB} ${embed_files} BINARY)
