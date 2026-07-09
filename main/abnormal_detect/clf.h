#pragma once
#include <cstdint>

/* Auto-generated contamination classifier - FP32 */
/* Feature dimension: 27 */
/* Non-zero weights: 26 / 27 */
/* Optimal threshold: 0.3828 */

#define CLF_FEAT_DIM 27
#define CLF_THRESHOLD_F32 0.38284451f

static const float clf_weights[27] = {0.01267777f, 0.03014492f, 0.01216272f, 0.00845152f, 0.09721028f, 0.00641176f, -0.05377035f, -0.10717390f, 0.00000000f, 3.64193915f, -3.35743345f, 6.15932894f, -3.46551697f, -0.85378130f, 2.79894463f, 15.82458279f, 13.20764327f, -2.26964617f, -5.37010933f, -1.56235756f, 56847.80990086f, -15.45751249f, 5985.18160283f, 0.01567832f, 20.02247427f, -0.05219604f, -0.03157563f};

static const float clf_bias = -2.39220071f;

/* Non-zero weight indices (for optional skip optimization) */
static const uint8_t clf_nonzero_idx[26] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};

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
