#pragma once
#include <cstdint>

/* Auto-generated contamination classifier - FP32 */
/* Feature dimension: 27 */
/* Non-zero weights: 26 / 27 */
/* Optimal threshold: 0.3499 */

#define CLF_FEAT_DIM 27
#define CLF_THRESHOLD_F32 0.34991205f

static const float clf_weights[27] = {0.01200115f, 0.03087182f, 0.01432094f, 0.01327503f, 0.09981485f, 0.00856688f, -0.05000380f, -0.11621535f, 4.77385194f, -1.56559973f, 33.70821090f, -40.93488992f, -3.57663560f, 5.35571399f, -25.70201973f, -192.38561151f, 3.16213832f, -9.62482590f, 0.00000000f, 8282.13480288f, 58827.56230362f, -12.89368295f, 5021.75188069f, 0.00293231f, 17.70569494f, -0.12100671f, -0.01347974f};

static const float clf_bias = -2.05448052f;

/* Non-zero weight indices (for optional skip optimization) */
static const uint8_t clf_nonzero_idx[26] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22, 23, 24, 25, 26};

/* Feature names (for debug) */
static const char* clf_feat_names[27] = {
    "sq_diff_r_mean",
    "sq_diff_g_mean",
    "sq_diff_b_mean",
    "sq_diff_r_std",
    "sq_diff_g_std",
    "sq_diff_b_std",
    "sq_diff_gray_mean",
    "sq_diff_gray_std",
    "sq_diff_hist_r0",
    "sq_diff_hist_r1",
    "sq_diff_hist_r2",
    "sq_diff_hist_r3",
    "sq_diff_hist_g0",
    "sq_diff_hist_g1",
    "sq_diff_hist_g2",
    "sq_diff_hist_g3",
    "sq_diff_hist_b0",
    "sq_diff_hist_b1",
    "sq_diff_hist_b2",
    "sq_diff_hist_b3",
    "sq_diff_area_norm",
    "sq_diff_aspect_ratio",
    "sq_diff_compactness",
    "feat_deviation",
    "similar_ratio",
    "nearest_dist",
    "local_density"
};

/*
 * Usage:
 *   float score = clf_bias;
 *   for (int i = 0; i < CLF_FEAT_DIM; i++)
 *       score += clf_weights[i] * (float)features[i];
 *   uint8_t is_contamination = (score > CLF_THRESHOLD_F32);
 */
