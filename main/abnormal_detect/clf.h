#ifndef CONTAMINATION_CLF_H
#define CONTAMINATION_CLF_H
#include <cstdint>

/* Auto-generated contamination classifier - FP32 */
/* Feature dimension: 27 */
/* Non-zero weights: 27 / 27 */

#define CLF_FEAT_DIM 27
// #define CLF_THRESHOLD_F32 -0.68518639f  /* logit space: score > this = contamination */
#define CLF_THRESHOLD_F32 2.386294f  /* logit space: score > this = contamination */

static const float clf_weights[27] = {0.01029833f, 0.02592125f, 0.01239237f, 0.01366511f, 0.09468852f, 0.00781100f, -0.04228352f, -0.11200173f, 2.99100541f, -0.42272673f, 30.53288542f, -38.93265316f, -2.68229461f, 4.63040236f, -22.96816002f, -170.24396399f, 6.95106794f, -13.71096123f, 6.12093763f, 9394.61970339f, 59009.84645290f, -13.18549624f, 5100.39467982f, 0.00358554f, -2.80504269f, -0.12980605f, -0.01512826f};

static const float clf_bias = -2.00635226f;

/* Non-zero weight indices (for optional skip optimization) */
static const uint8_t clf_nonzero_idx[27] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};

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

#endif /* CONTAMINATION_CLF_H */