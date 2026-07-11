#pragma once

#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include <cstddef>
#include <vector>

void extract_base_features(const dl::image::img_t &src,
                           const dl::detect::result_t &result,
                           float *base_feature);
void extract_mean_feature(float *base_feature,
                          float *mean_feature,
                          uint32_t size);
bool inference(const dl::image::img_t &src,
               const std::vector<dl::detect::result_t> &result,
               size_t index,
               float *base_feature,
               float *mean_feature);