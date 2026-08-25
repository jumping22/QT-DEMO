#ifndef SMOOTH_H
#define SMOOTH_H

#include <stddef.h>

/*
 * Real-time peak-cutting smoother.
 *
 * Streaming API: one output per input, no lookahead.
 * Offline API: centered opening + Gaussian (uses future samples).
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PEAKCUT_DEFAULT_RADIUS 8
#define PEAKCUT_DEFAULT_SIGMA  4.0f
#define PEAKCUT_MAX_RADIUS     48
#define PEAKCUT_MAX_SIGMA      16.0f
#define PEAKCUT_STREAM_MAX     97  /* 2 * PEAKCUT_MAX_RADIUS + 1 */

/* Offline (uses future samples). y may alias x. 0 on success, -1 on error. */
int peakcut_filter(const float *x, float *y, size_t n, int radius, float sigma);

/*
 * Real-time peak-cut smoother: one output per input, no lookahead.
 * Causal opening cuts narrow spikes using the past window; a 2-pole
 * low-pass then makes successive outputs a smooth curve.
 *
 * Hot path: no malloc. Window scan is O(radius).
 */
typedef struct {
    int radius;
    int win;
    float sigma;
    float alpha;
    float raw[PEAKCUT_STREAM_MAX];
    float eroded[PEAKCUT_STREAM_MAX];
    int iraw;
    int iero;
    int nraw;
    int nero;
    float z1;
    float z2;
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
