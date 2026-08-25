#ifndef SMOOTH_H
#define SMOOTH_H

/*
 * Causal, O(1)-per-sample smoothers for real-time streams.
 *
 * All filters use only the current sample and a few words of state.
 * There is no lookahead, no window copy, and no heap in the hot path.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float min_cutoff; /* Hz-like cutoff when the signal is slow (dt=1 => per-sample) */
    float beta;       /* speed adaptation: higher = less lag on fast edges */
    float d_cutoff;   /* cutoff used to filter the derivative */
    float x_hat;      /* last smoothed value */
    float dx_hat;     /* last smoothed derivative */
    int initialized;
} OneEuro;

typedef struct {
    float min_alpha;  /* smoothing when |error| is small, (0, 1] */
    float max_alpha;  /* smoothing when |error| is large, (0, 1] */
    float knee;       /* error (same units as x) at which alpha is mid-range */
    float y;
    int initialized;
} AdaptiveEma;

typedef struct {
    float alpha;      /* (0, 1], 1 = no smoothing */
    float y;
    int initialized;
} Ema;

/* min_cutoff=0.02, beta=0.08, d_cutoff=1.0 is a low-lag default when dt=1. */
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
