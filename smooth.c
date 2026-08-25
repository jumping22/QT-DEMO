#include "smooth.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float x, float lo, float hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

static float min_window(const float *v, size_t n, size_t i, int r)
{
    size_t a = (i > (size_t)r) ? i - (size_t)r : 0;
    size_t b = i + (size_t)r + 1;
    size_t k;
    float m;

    if (b > n) {
        b = n;
    }
    m = v[a];
    for (k = a + 1; k < b; k++) {
        if (v[k] < m) {
            m = v[k];
        }
    }
    return m;
}

static float max_window(const float *v, size_t n, size_t i, int r)
{
    size_t a = (i > (size_t)r) ? i - (size_t)r : 0;
    size_t b = i + (size_t)r + 1;
    size_t k;
    float m;

    if (b > n) {
        b = n;
    }
    m = v[a];
    for (k = a + 1; k < b; k++) {
        if (v[k] > m) {
            m = v[k];
        }
    }
    return m;
}

/* Morphological opening: erode then dilate. Cuts peaks, keeps wide valleys. */
static void morph_open(const float *x, float *y, size_t n, int radius)
{
    float *eroded;
    size_t i;

    eroded = (float *)malloc(n * sizeof(float));
    if (!eroded) {
        memcpy(y, x, n * sizeof(float));
        return;
    }
    for (i = 0; i < n; i++) {
        eroded[i] = min_window(x, n, i, radius);
    }
    for (i = 0; i < n; i++) {
        y[i] = max_window(eroded, n, i, radius);
    }
    free(eroded);
}

static void gaussian_smooth(const float *x, float *y, size_t n, float sigma)
{
    int r;
    int k;
    size_t i;
    double *kernel;
    double ksum;

    if (sigma <= 0.0f) {
        if (y != x) {
            memcpy(y, x, n * sizeof(float));
        }
        return;
    }

    r = (int)ceil((double)sigma * 3.0);
    if (r < 1) {
        r = 1;
    }
    kernel = (double *)malloc((size_t)(2 * r + 1) * sizeof(double));
    if (!kernel) {
        if (y != x) {
            memcpy(y, x, n * sizeof(float));
        }
        return;
    }

    ksum = 0.0;
    for (k = -r; k <= r; k++) {
        double u = (double)k / (double)sigma;
        double w = exp(-0.5 * u * u);
        kernel[k + r] = w;
        ksum += w;
    }
    for (k = 0; k < 2 * r + 1; k++) {
        kernel[k] /= ksum;
    }

    for (i = 0; i < n; i++) {
        double acc = 0.0;
        double wsum = 0.0;
        int t;
        for (t = -r; t <= r; t++) {
            long idx = (long)i + (long)t;
            if (idx < 0 || idx >= (long)n) {
                continue;
            }
            acc += (double)x[idx] * kernel[t + r];
            wsum += kernel[t + r];
        }
        y[i] = (float)(acc / wsum);
    }
    free(kernel);
}

int peakcut_filter(const float *x, float *y, size_t n, int radius, float sigma)
{
    float *opened;

    if (!x || !y || n == 0) {
        return -1;
    }
    if (radius < 1) {
        radius = 1;
    }
    if (radius > PEAKCUT_MAX_RADIUS) {
        radius = PEAKCUT_MAX_RADIUS;
    }
    if (sigma > PEAKCUT_MAX_SIGMA) {
        sigma = PEAKCUT_MAX_SIGMA;
    }

    opened = (float *)malloc(n * sizeof(float));
    if (!opened) {
        return -1;
    }
    morph_open(x, opened, n, radius);
    gaussian_smooth(opened, y, n, sigma);
    free(opened);
    return 0;
}

/*
 * Critically-damped lag (Unity SmoothDamp). dt = 1 sample.
 * Smaller smooth_time → lower lag, slightly less rounding.
 */
static float smooth_damp(float current, float target, float *vel, float smooth_time)
{
    const float dt = 1.0f;
    float st;
    float omega;
    float x;
    float expn;
    float change;
    float original;
    float temp;
    float out;

    st = smooth_time > 1e-4f ? smooth_time : 1e-4f;
    omega = 2.0f / st;
    x = omega * dt;
    expn = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    original = target;
    change = current - target;
    temp = (*vel + omega * change) * dt;
    *vel = (*vel - omega * temp) * expn;
    out = target + (change + temp) * expn;
    if ((original - current > 0.0f) == (out > original)) {
        out = original;
        *vel = 0.0f;
    }
    return out;
}

void peakcut_stream_init(PeakCutStream *s, int radius, float sigma)
{
    memset(s, 0, sizeof(*s));
    if (radius < 1) {
        radius = 1;
    }
    if (radius > PEAKCUT_MAX_RADIUS) {
        radius = PEAKCUT_MAX_RADIUS;
    }
    if (sigma < 0.5f) {
        sigma = 0.5f;
    }
    if (sigma > PEAKCUT_MAX_SIGMA) {
        sigma = PEAKCUT_MAX_SIGMA;
    }
    s->hold = radius;
    s->margin = PEAKCUT_DEFAULT_MARGIN;
    /* Larger sigma → slower rise tracking, rounder curve. Drops stay fast. */
    s->st_up = 1.4f + 0.55f * sigma;
    s->st_dn = 0.85f + 0.08f * sigma;
    if (s->st_dn < 0.7f) {
        s->st_dn = 0.7f;
    }
}

float peakcut_stream_update(PeakCutStream *s, float x)
{
    float gated;
    float st;

    if (!s->initialized) {
        s->initialized = 1;
        s->y = x;
        s->vel = 0.0f;
        s->high_count = 0;
        return x;
    }

    /*
     * Direction + duration gate (no window):
     *   x well above y for fewer than `hold` samples → spike, stay
     *   same, but confirmed → real rise, catch up
     *   x not well above y (includes drops) → track immediately
     */
    if (x > s->y + s->margin) {
        s->high_count++;
        if (s->high_count >= s->hold) {
            gated = x;
            /* Confirmed rise: faster than plateau, slower than a drop. */
            st = 0.55f * s->st_up + 0.45f * s->st_dn;
        } else {
            /* Tiny leak so a long ramp does not freeze then jump. */
            gated = s->y + 0.08f * (x - s->y);
            st = s->st_up;
        }
    } else {
        s->high_count = 0;
        gated = x;
        st = (x < s->y) ? s->st_dn : s->st_up;
    }

    s->y = smooth_damp(s->y, gated, &s->vel, st);
    return s->y;
}

static float alpha_from_cutoff(float cutoff, float dt)
{
    float tau;
    float a;

    if (cutoff <= 0.0f || dt <= 0.0f) {
        return 1.0f;
    }
    tau = 1.0f / (2.0f * (float)M_PI * cutoff);
    a = 1.0f / (1.0f + tau / dt);
    return clampf(a, 0.0f, 1.0f);
}

static float lowpass(float prev, float x, float alpha)
{
    return alpha * x + (1.0f - alpha) * prev;
}

void one_euro_init(OneEuro *f, float min_cutoff, float beta, float d_cutoff)
{
    f->min_cutoff = min_cutoff;
    f->beta = beta;
    f->d_cutoff = d_cutoff;
    f->x_hat = 0.0f;
    f->dx_hat = 0.0f;
    f->initialized = 0;
}

float one_euro_update(OneEuro *f, float x, float dt)
{
    float dx;
    float edx;
    float cutoff;
    float a_d;
    float a_x;

    if (dt <= 0.0f) {
        dt = 1.0f;
    }

    if (!f->initialized) {
        f->initialized = 1;
        f->x_hat = x;
        f->dx_hat = 0.0f;
        return x;
    }

    dx = (x - f->x_hat) / dt;
    a_d = alpha_from_cutoff(f->d_cutoff, dt);
    f->dx_hat = lowpass(f->dx_hat, dx, a_d);

    edx = f->dx_hat >= 0.0f ? f->dx_hat : -f->dx_hat;
    cutoff = f->min_cutoff + f->beta * edx;
    a_x = alpha_from_cutoff(cutoff, dt);
    f->x_hat = lowpass(f->x_hat, x, a_x);
    return f->x_hat;
}

void aema_init(AdaptiveEma *f, float min_alpha, float max_alpha, float knee)
{
    f->min_alpha = clampf(min_alpha, 0.0f, 1.0f);
    f->max_alpha = clampf(max_alpha, 0.0f, 1.0f);
    if (f->max_alpha < f->min_alpha) {
        f->max_alpha = f->min_alpha;
    }
    f->knee = knee > 1e-6f ? knee : 1e-6f;
    f->y = 0.0f;
    f->initialized = 0;
}

float aema_update(AdaptiveEma *f, float x)
{
    float err;
    float aerr;
    float mix;
    float alpha;

    if (!f->initialized) {
        f->initialized = 1;
        f->y = x;
        return x;
    }

    err = x - f->y;
    aerr = err >= 0.0f ? err : -err;
    mix = aerr / (aerr + f->knee);
    alpha = f->min_alpha + (f->max_alpha - f->min_alpha) * mix;
    f->y += alpha * err;
    return f->y;
}

void ema_init(Ema *f, float alpha)
{
    f->alpha = clampf(alpha, 0.0f, 1.0f);
    f->y = 0.0f;
    f->initialized = 0;
}

float ema_update(Ema *f, float x)
{
    if (!f->initialized) {
        f->initialized = 1;
        f->y = x;
        return x;
    }
    f->y += f->alpha * (x - f->y);
    return f->y;
}
