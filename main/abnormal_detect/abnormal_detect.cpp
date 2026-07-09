#include "dl_image.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>

#include "abnormal_detect.hpp"
#include "clf.h"

void extract_base_features(const dl::image::img_t &src,
                           const dl::detect::result_t &result,
                           float *base_feature)
{
    auto img = static_cast<const uint8_t *>(src.data);
    uint32_t color_monent_temp[8];
    float color_monent[8];
    // uint16_t color_histogram_temp[3][4];
    float color_histogram[3][4];
    const int &left_x = result.box[0];
    const int &left_y = result.box[1];
    const int &right_x = result.box[2];
    const int &right_y = result.box[3];
    memset(color_monent_temp, 0, sizeof(color_monent_temp));
    memset(color_monent, 0, sizeof(color_monent));
    // memset(color_histogram_temp, 0, sizeof(color_histogram_temp));
    memset(color_histogram, 0, sizeof(color_histogram));

    for (uint16_t x = left_x; x < right_x; x++)
    {
        for (uint16_t y = left_y; y < right_y; y++)
        {
            uint8_t r = *(img + (src.width * y + x));
            uint8_t g = *(img + (src.width * y + x) + 1);
            uint8_t b = *(img + (src.width * y + x) + 2);

            // 计算颜色矩
            color_monent_temp[0] += r;
            color_monent_temp[1] += r * r;
            color_monent_temp[2] += g;
            color_monent_temp[3] += g * g;
            color_monent_temp[4] += b;
            color_monent_temp[5] += b * b;
            uint16_t gray = (299 * r + 587 * g + 114 * b) / 1000;
            color_monent_temp[6] += gray;
            color_monent_temp[7] += gray * gray;

            // 计算直方图
            color_histogram[0][r >> 6] += 1;
            color_histogram[1][g >> 6] += 1;
            color_histogram[1][b >> 6] += 1;
        }
    }
    uint16_t area_size = (right_x - left_x) * (right_y - left_y);
    for (uint16_t i = 0; i < 8; i += 2)
        color_monent[i] = static_cast<float>(color_monent_temp[i]) / area_size;
    for (uint16_t i = 1; i < 8; i += 2)
    {
        float total_var = static_cast<float>(color_monent_temp[i] - color_monent_temp[i - 1] * color_monent_temp[i - 1]);
        color_monent[i] = std::sqrt(total_var) / area_size;
    }
    for (uint16_t i = 0; i < 3; i++)
        for (uint16_t j = 1; j < 4; j++)
            color_histogram[i][j] /= area_size;

    float area = static_cast<float>(area_size) / (src.width * src.height);
    float aspect_ratio = static_cast<float>(right_x - left_x) / (right_y - right_x);
    float compactness = 4 * std::numbers::pi * (src.width * src.height) / (2 * src.width + 2 * src.height);

    // 总长度：23
    memcpy(base_feature, color_monent, sizeof(color_monent));
    memcpy(base_feature + sizeof(color_monent), color_histogram, sizeof(color_histogram));
    *(base_feature + sizeof(color_monent) + sizeof(color_monent)) = area;
    *(base_feature + sizeof(color_monent) + sizeof(color_monent) + 1) = aspect_ratio;
    *(base_feature + sizeof(color_monent) + sizeof(color_monent) + 2) = compactness;
}

void extract_mean_feature(float *base_feature,
                          float *mean_feature,
                          uint32_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        for (int j = 0; j < 23; j++)
            *(mean_feature + j) += *(base_feature + 23 * i + j);
    }
    for (int i = 0; i < 23; i++)
        *(mean_feature + i) /= size;
}

bool inference(const dl::image::img_t &src,
               const std::vector<dl::detect::result_t> &result,
               size_t index,
               float *base_feature,
               float *mean_feature)
{
    float input_feature[27];
    memset(input_feature, 0, sizeof(input_feature));
    int ori_center_x = (result[index].box[0] + result[index].box[2]) / 2;
    int ori_center_y = (result[index].box[1] + result[index].box[3]) / 2;

    for (int i = 0; i < 23; i++)
    {
        float diff = *(base_feature + 23 * index + i) - *(mean_feature + i);
        input_feature[i] = diff * diff;
        input_feature[23] += diff * diff;
    }
    input_feature[23] = std::sqrt(input_feature[23]);

    float similar_cnt = 0;
    float nearest_dist2 = src.width * src.width + src.height * src.height;
    int local_density = 0;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (i == index)
            continue;

        float distance2 = 0;
        for (size_t j = 9; j < 23; j++)
        {
            float diff = *(base_feature + 23 * i + j) - *(base_feature + 23 * index + j);
            distance2 += diff * diff;
        }
        if (distance2 <= 0.5 * 0.5)
            similar_cnt++;

        int center_x = (result[i].box[0] + result[i].box[2]) / 2;
        int center_y = (result[i].box[1] + result[i].box[3]) / 2;
        int colony_distance2 = (center_x - ori_center_x) * (center_x - ori_center_x) + (center_y - ori_center_y) * (center_y - ori_center_y);
        nearest_dist2 = colony_distance2 < nearest_dist2 ? colony_distance2 : nearest_dist2;
        if (colony_distance2 < 0.1 * 0.1 * src.width * src.height)
            local_density++;
    }
    similar_cnt /= src.width * src.height;
    input_feature[24] = similar_cnt;
    input_feature[25] = std::sqrt(nearest_dist2);
    input_feature[26] = local_density;

    // 推理
    float score = clf_bias;
    for (int i = 0; i < CLF_FEAT_DIM; i++)
        score += clf_weights[i] * input_feature[i];
    return (score > CLF_THRESHOLD_F32);
}
