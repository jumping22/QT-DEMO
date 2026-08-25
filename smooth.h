#ifndef SMOOTH_H
#define SMOOTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PEAKCUT_DEFAULT_RADIUS 16
#define PEAKCUT_DEFAULT_SIGMA  5.0f
#define PEAKCUT_MAX_RADIUS     48
#define PEAKCUT_MAX_SIGMA      16.0f
#define PEAKCUT_DEFAULT_MARGIN 6.0f

/* Offline (uses future samples). y may alias x. 0 on success, -1 on error. */
int peakcut_filter(const float *x, float *y, size_t n, int radius, float sigma);

/*
 * Real-time smoother: one output per input, O(1), no lookahead.
 *
 * 1) Two-sided deadband + duration gate: ignore short oscillations;
 *    large moves confirm after 2 samples.
 * 2) Online min-jerk toward (dest, dest_vel): tracks a moving target
 *    instead of always planning to stop, so lag stays low. Horizon
 *    grows with sqrt(|error|) plus a floor, so plateaus stay quiet
 *    and long steps still ease in.
 * 3) Sticky follow only unlocks on a reversal (periodic teeth) or a
 *    true long-term flat. Mid-rise / mid-drop pauses do not lock a
 *    staircase platform; a small cruise velocity keeps the S-curve
 *    moving while the slow trend is still live.
 */
typedef struct {
    int hold;
    float dead;
    float big_up;
    float big_dn;
    float leak;
    float t_min;
    float t_sqrt;
    float t_max;
    float j_max;
    float v_beta;
    float v_lim;
    float settle_span;
    int settle_need;
    float reverse_need;
    int reverse_hold;
    float slow_alpha;
    float trend_eps;
    float cruise;
    float ignore_up;
    float ignore_dn;
    float y;
    float vel;
    float acc;
    float dest_prev;
    float dest_vel;
    float x_slow;
    float ext;
    int move_dir;
    int reverse_count;
    float hist[PEAKCUT_MAX_RADIUS];
    int hist_len;
    int hist_pos;
    int sticky;
    int settle_count;
    int up_count;
    int dn_count;
    int initialized;
} PeakCutStream;

void peakcut_stream_init(PeakCutStream *s, int radius, float sigma);
float peakcut_stream_update(PeakCutStream *s, float x);

typedef struct {
    float min_cutoff;
    float beta;
    float d_cutoff;
    float x_hat;
    float dx_hat;
    int initialized;
} OneEuro;

typedef struct {
    float min_alpha;
    float max_alpha;
    float knee;
    float y;
    int initialized;
} AdaptiveEma;

typedef struct {
    float alpha;
    float y;
    int initialized;
} Ema;

void one_euro_init(OneEuro *f, float min_cutoff, float beta, float d_cutoff);
float one_euro_update(OneEuro *f, float x, float dt);

void aema_init(AdaptiveEma *f, float min_alpha, float max_alpha, float knee);
float aema_update(AdaptiveEma *f, float x);

void ema_init(Ema *f, float alpha);
float ema_update(Ema *f, float x);

#ifdef __cplusplus
}
#endif

#endif
