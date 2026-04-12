#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include "dl_image_process.hpp"
#include "esp_heap_caps.h"
#include "dl_image_jpeg.hpp"
#include "dl_tensor_base.hpp"
#include "classes.hpp"

struct Detection {
    float x1, y1, x2, y2;
    float score;
    int class_id;
};

class Yolo26Processor {
private:
    // --- constants ---
    const int num_classes = 1;
    const int strides[3] = {8, 16, 32};
    
    // --- State (Calculated/Configured) ---
    std::vector<int> grid_sizes; // Calculated in preprocess
    int target_k;
    float conf_thresh;
    const char** class_names;
    
    // --- Optimization ---
    // Lookup Table for Quantization
    // Stores pre-calculated (pixel / 255.0 * 128) values for all 256 inputs.
    // Located in internal SRAM (part of class instance on stack).
    int8_t quantization_lut[256];

    // --- Helpers ---
    inline float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    template <typename T>
    inline float dequantize_val(T val, float scale) {
        return val * scale;
    }

public:
    /**
     * @brief Constructor.
     * Initializes configuration state and Pre-calculates Quantization LUT.
     * 
     * @param k Max detections (Default: 32)
     * @param thresh Confidence threshold (Default: 0.10f)
     * @param classes Class name array (Default: coco_classes)
     */
    Yolo26Processor(int k, float thresh, const char** classes): target_k(k), conf_thresh(thresh), class_names(classes) {
        // Reserve memory for grids
        grid_sizes.resize(3);
        
        // Calculate Quantization LUT (Lossless)
        // Recovers the exact precision of floating point normalization.
        // Scale 128 correspondes to exponent -7 (which is validated in preprocess).
        // Formula: round( (pixel / 255.0) * 128 )
        for (int i = 0; i < 256; i++) {
            float normalized = i / 255.0f;
            float scaled = normalized * 128.0f; 
            int val = (int)std::round(scaled);
            
            // Clamp to int8 range [-128, 127]
            if (val > 127) val = 127;
            if (val < -128) val = -128;
            
            quantization_lut[i] = (int8_t)val;
        }
    }
    
    ~Yolo26Processor() {
    }

    /**
     * @brief Preprocesses image and updates internal state (grid_sizes).
     * 
     * @param img Input image (RGB888)
     * @param inputs Model input map (used to get tensor data and shape)
     */
    void preprocess(const dl::image::img_t& img, const std::map<std::string, dl::TensorBase*>& inputs) {
        // 1. Get the first input tensor
        if (inputs.empty()) return;
        dl::TensorBase* input_tensor = inputs.begin()->second;

        // 2. Validate Exponent for LUT Optimization
        // Logic: 8 (uint8 bits) + exponent (usually -7) should equal 1
        int shift_check = 8 + input_tensor->exponent;
        if (shift_check != 1) {
             printf("[Yolo26Processor] Error: Model exponent %d not compatible with optimization (Expected -7)\n", input_tensor->exponent);
        }
        assert(shift_check == 1);

        // 3. Calculate and Store Grid Sizes
        int input_w = input_tensor->shape[2]; 
        for(int i=0; i<3; i++) {
            grid_sizes[i] = input_w / strides[i];
        }

        // 4. Quantize using LUT (Fast & Lossless)
        uint8_t* rgb_data = (uint8_t*)img.data;
        int8_t* raw_input = (int8_t*)input_tensor->data;
        int total_pixels = img.width * img.height * 3;

        for (int i = 0; i < total_pixels; i++) {
            // LUT lookup recovers exact floating point precision without the cost
            raw_input[i] = quantization_lut[rgb_data[i]];
        }
    }

    /**
     * @brief Post-processes outputs using stored state.
     * 
     * OPTIMIZATION: QUANTIZED THRESHOLDING
     * Instead of dequantizing and running sigmoid() for every one of the 672,000 class scores (which is slow),
     * we calculate a raw INT8 threshold for each layer.
     * We then filter candidates in the integer domain: if (raw_int8 <= thresh_int8) continue;
     * This skips >99% of floating point math for background pixels.
     * 
     * @param outputs Map of model outputs
     * @return std::vector<Detection> 
     */
    std::vector<Detection> postprocess(const std::map<std::string, dl::TensorBase*>& outputs) {
        // Ensure grid_sizes are ready
        if (grid_sizes.empty() || grid_sizes[0] == 0) {
             printf("[Yolo26Processor] Error: Grid sizes not initialized. Call preprocess() first.\n");
             return {};
        }

        dl::TensorBase* p3_box = outputs.at("one2one_p3_box");
        dl::TensorBase* p4_box = outputs.at("one2one_p4_box");
        dl::TensorBase* p5_box = outputs.at("one2one_p5_box");
        dl::TensorBase* p3_cls = outputs.at("one2one_p3_cls");
        dl::TensorBase* p4_cls = outputs.at("one2one_p4_cls");
        dl::TensorBase* p5_cls = outputs.at("one2one_p5_cls");

        std::vector<Detection> candidates;
        candidates.reserve(target_k * 2);

        dl::TensorBase* boxes[] = {p3_box, p4_box, p5_box};
        dl::TensorBase* clss[] = {p3_cls, p4_cls, p5_cls};
        dl::dtype_t dtype = p3_box->dtype;

        for (int i = 0; i < 3; i++) {
            int stride = strides[i];
            int grid_h = grid_sizes[i]; // Use stored grid size
            int grid_w = grid_sizes[i];

            float box_scale = std::pow(2.0f, boxes[i]->exponent);
            float cls_scale = std::pow(2.0f, clss[i]->exponent);
            void* raw_box = boxes[i]->data;
            void* raw_cls = clss[i]->data;

            // --- Optimization: Calculate INT8 Threshold for this layer ---
            // raw_thresh = -ln(1/conf_thresh - 1)
            // int8_thresh = raw_thresh / cls_scale
            float raw_thresh_float = -std::log(1.0f / conf_thresh - 1.0f);
            int8_t cls_thresh_int8 = 0;
            // Note: We assume int8 here as per optimization plan. Check dtype if int16 needed.
            // Current models are int8.
            cls_thresh_int8 = (int8_t)std::floor(raw_thresh_float / cls_scale);

            for (int h = 0; h < grid_h; h++) {
                for (int w = 0; w < grid_w; w++) {
                    int pixel_idx = (h * grid_w) + w; // NHWC
                    int cls_offset = pixel_idx * num_classes;
                    
                    float max_score = -1.0f;
                    int best_cls_id = -1;

                    // Class Score Loop
                    for (int c = 0; c < num_classes; c++) {
                        // --- OPTIMIZATION START ---
                        // 1. Fast Integer Check
                        if (dtype == dl::DATA_TYPE_INT8) {
                            int8_t raw_val_int8 = ((int8_t*)raw_cls)[cls_offset + c];
                            if (raw_val_int8 <= cls_thresh_int8) continue; // SKIP FLOAT MATH
                        }
                        // --- OPTIMIZATION END ---

                        float raw_val = 0.0f;
                        if (dtype == dl::DATA_TYPE_INT8) {
                            raw_val = dequantize_val(((int8_t*)raw_cls)[cls_offset + c], cls_scale);
                        } else {
                            raw_val = dequantize_val(((int16_t*)raw_cls)[cls_offset + c], cls_scale);
                        }
                        
                        float score = sigmoid(raw_val);
                        if (score > max_score) {
                            max_score = score;
                            best_cls_id = c;
                        }
                    }

                    if (max_score < conf_thresh) continue;

                    // Decode Box
                    int box_offset = pixel_idx * 4;
                    float d_l, d_t, d_r, d_b;

                    if (dtype == dl::DATA_TYPE_INT8) {
                        int8_t* ptr = (int8_t*)raw_box;
                        d_l = dequantize_val(ptr[box_offset + 0], box_scale);
                        d_t = dequantize_val(ptr[box_offset + 1], box_scale);
                        d_r = dequantize_val(ptr[box_offset + 2], box_scale);
                        d_b = dequantize_val(ptr[box_offset + 3], box_scale);
                    } else {
                        int16_t* ptr = (int16_t*)raw_box;
                        d_l = dequantize_val(ptr[box_offset + 0], box_scale);
                        d_t = dequantize_val(ptr[box_offset + 1], box_scale);
                        d_r = dequantize_val(ptr[box_offset + 2], box_scale);
                        d_b = dequantize_val(ptr[box_offset + 3], box_scale);
                    }

                    float cx = w + 0.5f;
                    float cy = h + 0.5f;
                    float x1 = (cx - d_l) * stride;
                    float y1 = (cy - d_t) * stride;
                    float x2 = (cx + d_r) * stride;
                    float y2 = (cy + d_b) * stride;

                    candidates.push_back({x1, y1, x2, y2, max_score, best_cls_id});
                }
            }
        }

        // Global Sort
        std::sort(candidates.begin(), candidates.end(), [](const Detection& a, const Detection& b) {
            return a.score > b.score;
        });

        if (candidates.size() > target_k) {
            candidates.resize(target_k);
        }

        return candidates;
    }
};
