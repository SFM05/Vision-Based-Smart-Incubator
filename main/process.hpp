#pragma once
#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "dl_model_base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "yolo26.hpp"
#include <cstdint>
#include <vector>

bool capture_init();
void configure_isp();

struct capture_process_args
{
    dl::image::img_t &cropped_buf; // 1080*1080
    QueueHandle_t &start_inference;
    SemaphoreHandle_t &inference_end;
    SemaphoreHandle_t &inference_capture_mutex;
    SemaphoreHandle_t &request_upload;
};
struct frame_meta_data
{
    char time[20];
    uint8_t plate_id;
};
void capture_process(void *argv);

struct inference_process_args
{
    dl::image::img_t &cropped_buf; // 1080*1080
    dl::Model *model;
    YOLO26 &processor;
    std::vector<dl::detect::result_t> result;
    QueueHandle_t &start_inference;
    SemaphoreHandle_t &inference_end;
    SemaphoreHandle_t &capture_inference_mutex;
};
void inference_process(void *argv);

struct upload_request_process_args{
    QueueHandle_t &request_upload;
    std::vector<dl::detect::result_t> result;
};
void upload_request_process(void *argv);

struct upload_process_args{
    QueueHandle_t msg_queue;
};
void upload_process(void *args);

// struct draw_box_process_args
// {
//     dl::image::img_t &cropped_buf;
//     std::vector<dl::detect::result_t> result;
// };
// void draw_box_process(void *argv);
