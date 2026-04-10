set(srcs "main.cpp" "camera/camera_driver.c" "lcd/lcd_driver.c" "pipeline/video_pipeline.c")
set(requires esp_lcd driver esp_video esp_lcd_ek79007 freertos)
idf_component_register(SRCS ${srcs}
                      REQUIRES ${requires}
                      INCLUDE_DIRS "." "camera" "lcd" "pipeline")
