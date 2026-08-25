#ifndef SMOOTH_H
#define SMOOTH_H

#include <stddef.h>

/*
 * Peak-cutting smoother: morphological opening (cuts narrow spikes)
 * followed by a Gaussian low-pass (makes a C-ish curve).
 *
 * Opening with radius R removes peaks narrower than 2R+1 samples and
 * leaves slower trends (the two wide valleys in the demo series).
 */

#ifdef __cplusplus
extern "C" {
#endif

#define PEAKCUT_DEFAULT_RADIUS 8
#define PEAKCUT_DEFAULT_SIGMA  4.0f
#define PEAKCUT_MAX_RADIUS     48
#define PEAKCUT_MAX_SIGMA      16.0f

/* y may alias x. Returns 0 on success, -1 on bad args / OOM. */
int peakcut_filter(const float *x, float *y, size_t n, int radius, float sigma);

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
