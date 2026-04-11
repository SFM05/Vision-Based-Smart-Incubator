#pragma once

#include "dl_model_base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "yolo26.hpp"

void yolo_inference(dl::Model *model,
                    Yolo26Processor &processor,
                    const uint8_t *jpg_start,
                    const uint8_t *jpg_end)
{
    // 1. Decode JPEG
    auto img = processor.decode_jpeg(jpg_start, (size_t)(jpg_end - jpg_start));

    auto resized_img = processor.resize(img, model->get_inputs());

    // 2. Preprocess (Measure Time)
    TickType_t start_pre = xTaskGetTickCount();
    processor.preprocess(resized_img, model->get_inputs());
    TickType_t end_pre = xTaskGetTickCount();

    // 3. Inference (Measure Time)
    TickType_t start_inf = xTaskGetTickCount();
    model->run();
    TickType_t end_inf = xTaskGetTickCount();

    // 4. Post-Process (Measure Time)
    TickType_t start_post = xTaskGetTickCount();
    auto results = processor.postprocess(model->get_outputs());
    TickType_t end_post = xTaskGetTickCount();

    // Calculate Latencies
    uint32_t lat_pre = (end_pre - start_pre) * portTICK_PERIOD_MS;
    uint32_t lat_inf = (end_inf - start_inf) * portTICK_PERIOD_MS;
    uint32_t lat_post = (end_post - start_post) * portTICK_PERIOD_MS;

    printf("Timings:\n");
    printf("  Pre-process:  %lu ms\n", (unsigned long)lat_pre);
    printf("  Inference:    %lu ms\n", (unsigned long)lat_inf);
    printf("  Post-process: %lu ms\n", (unsigned long)lat_post);
    printf("  Total:        %lu ms\n", (unsigned long)(lat_pre + lat_inf + lat_post));

    printf("total objects: %u", (unsigned)results.size());
    printf("\n--- Top Detections ---\n");
    int count = 0;
    for (const auto &det : results)
    {
        // if (count >= 5)
        //     break;

        const char *class_name = "unknown";
        if (det.class_id >= 0 && det.class_id < 80)
        {
            class_name = names[det.class_id];
        }

        printf("Det %d: %s (%.2f%%) | Box: [%.1f, %.1f, %.1f, %.1f]\n",
               count + 1,
               class_name,
               det.score * 100.0f,
               det.x1, det.y1, det.x2, det.y2);
        count++;
    }
    if (results.empty())
    {
        printf("No detections above threshold %.2f\n", YOLO_CONF_THRESH);
    }

    if (resized_img.data != img.data)
    {
        heap_caps_free(resized_img.data);
    }
    heap_caps_free(img.data);
}
