#ifndef SMOOTH_H
#define SMOOTH_H

#include "pressure_filter.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PEAKCUT_DEFAULT_RADIUS PRESSURE_FILTER_HOLD_DEF
#define PEAKCUT_DEFAULT_SIGMA  PRESSURE_FILTER_SMOOTH_DEF
#define PEAKCUT_MAX_RADIUS     PRESSURE_FILTER_HIST_MAX
#define PEAKCUT_MAX_SIGMA      16.0f
#define PEAKCUT_DEFAULT_MARGIN 6.0f

typedef PressureFilter PeakCutStream;

void peakcut_stream_init(PeakCutStream *s, int radius, float sigma);
float peakcut_stream_update(PeakCutStream *s, float x);

/* Offline (uses future samples and malloc). Not for MCU. */
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
