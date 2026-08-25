#ifndef SMOOTH_H
#define SMOOTH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PEAKCUT_DEFAULT_RADIUS 7
#define PEAKCUT_DEFAULT_SIGMA  3.5f
#define PEAKCUT_MAX_RADIUS     48
#define PEAKCUT_MAX_SIGMA      16.0f
#define PEAKCUT_DEFAULT_MARGIN 3.0f

/* Offline (uses future samples). y may alias x. 0 on success, -1 on error. */
int peakcut_filter(const float *x, float *y, size_t n, int radius, float sigma);

/*
 * Real-time smoother: one output per input, O(1), no lookahead, no buffers.
 *
 * Upward bursts shorter than `radius` samples are treated as spikes and
 * ignored. Real drops and confirmed rises are tracked with an asymmetric
 * SmoothDamp so successive outputs stay a smooth curve with low lag.
 */
typedef struct {
    int hold;
    float margin;
    float st_up;
    float st_dn;
    float y;
    float vel;
    int high_count;
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
