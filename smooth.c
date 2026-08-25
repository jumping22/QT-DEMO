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

void peakcut_stream_init(PeakCutStream *s, int radius, float sigma)
{
    memset(s, 0, sizeof(*s));
    if (radius < 1) {
        radius = 1;
    }
    if (radius > PEAKCUT_MAX_RADIUS) {
        radius = PEAKCUT_MAX_RADIUS;
    }
    if (sigma < 0.8f) {
        sigma = 0.8f;
    }
    if (sigma > PEAKCUT_MAX_SIGMA) {
        sigma = PEAKCUT_MAX_SIGMA;
    }
    s->hold = radius;
    /* Deadband covers typical tooth height so the gate never arms on a plateau. */
    s->dead = 3.5f + 0.5f * sigma;
    s->big_up = 8.0f + sigma;
    s->big_dn = 6.0f + 0.8f * sigma;
    s->leak = 0.06f / sigma;
    if (s->leak > 0.035f) {
        s->leak = 0.035f;
    }
    if (s->leak < 0.010f) {
        s->leak = 0.010f;
    }
    s->st_min = 2.3f - 0.16f * sigma;
    if (s->st_min > 2.2f) {
        s->st_min = 2.2f;
    }
    if (s->st_min < 1.2f) {
        s->st_min = 1.2f;
    }
    s->st_max = 8.0f + 1.6f * sigma;
    if (s->st_max > 24.0f) {
        s->st_max = 24.0f;
    }
    s->knee = 4.0f + 0.8f * sigma;
    s->down_st = 0.58f - 0.032f * sigma;
    if (s->down_st > 0.55f) {
        s->down_st = 0.55f;
    }
    if (s->down_st < 0.38f) {
        s->down_st = 0.38f;
    }
    /* Local range below this while close to y → leave sticky follow. */
    s->settle_span = 4.0f + 0.6f * sigma;
    s->settle_need = 6;
}

/* Unity / Game Programming Gems 4 critically-damped smoother. dt = 1 sample. */
static float smooth_damp(float current, float target, float *vel, float smooth_time)
{
    float omega;
    float x;
    float exp2;
    float change;
    float temp;
    float out;

    if (smooth_time < 0.4f) {
        smooth_time = 0.4f;
    }
    omega = 2.0f / smooth_time;
    x = omega; /* dt = 1 */
    exp2 = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    change = current - target;
    temp = (*vel + omega * change);
    *vel = (*vel - omega * temp) * exp2;
    out = target + (change + temp) * exp2;
    return out;
}

static void hist_reset(PeakCutStream *s)
{
    s->hist_len = 0;
    s->hist_pos = 0;
    s->settle_count = 0;
}

static void hist_push(PeakCutStream *s, float x)
{
    int cap = s->hold;
    if (cap < 1) {
        cap = 1;
    }
    if (cap > PEAKCUT_MAX_RADIUS) {
        cap = PEAKCUT_MAX_RADIUS;
    }
    s->hist[s->hist_pos] = x;
    s->hist_pos++;
    if (s->hist_pos >= cap) {
        s->hist_pos = 0;
    }
    if (s->hist_len < cap) {
        s->hist_len++;
    }
}

static float hist_range(const PeakCutStream *s)
{
    int i;
    float mn;
    float mx;

    if (s->hist_len <= 0) {
        return 0.0f;
    }
    mn = s->hist[0];
    mx = s->hist[0];
    for (i = 1; i < s->hist_len; i++) {
        if (s->hist[i] < mn) {
            mn = s->hist[i];
        }
        if (s->hist[i] > mx) {
            mx = s->hist[i];
        }
    }
    return mx - mn;
}

static void enter_sticky(PeakCutStream *s, float x)
{
    s->sticky = 1;
    s->up_count = 0;
    s->dn_count = 0;
    hist_reset(s);
    hist_push(s, x);
}

float peakcut_stream_update(PeakCutStream *s, float x)
{
    float gated;
    float err;
    float aerr;
    float mix;
    float st;
    float d;
    float ad;
    int need;
    int confirmed;

    if (!s->initialized) {
        s->initialized = 1;
        s->y = x;
        s->vel = 0.0f;
        s->up_count = 0;
        s->dn_count = 0;
        s->sticky = 0;
        hist_reset(s);
        return x;
    }

    d = x - s->y;
    ad = d >= 0.0f ? d : -d;

    /*
     * Sticky follow: after a real move is confirmed, keep tracking x
     * until the recent window looks like a plateau (small range and
     * close to y). That stops the deadband from staircase-catching
     * on ramps: one confirm, then a continuous follow to the next flat.
     */
    if (s->sticky) {
        gated = x;
        hist_push(s, x);
        if (hist_range(s) <= s->settle_span && ad <= s->dead) {
            s->settle_count++;
        } else {
            s->settle_count = 0;
        }
        if (s->settle_count >= s->settle_need) {
            s->sticky = 0;
            hist_reset(s);
        }
    } else {
        confirmed = 0;
        if (ad <= s->dead) {
            s->up_count = 0;
            s->dn_count = 0;
            gated = s->y + s->leak * d;
        } else if (d > 0.0f) {
            s->up_count++;
            s->dn_count = 0;
            need = (d >= s->big_up) ? 2 : s->hold;
            confirmed = s->up_count >= need;
            gated = confirmed ? x : (s->y + s->leak * d);
        } else {
            s->dn_count++;
            s->up_count = 0;
            need = ((-d) >= s->big_dn) ? 2 : s->hold;
            confirmed = s->dn_count >= need;
            gated = confirmed ? x : (s->y + s->leak * d);
        }
        if (confirmed) {
            enter_sticky(s, x);
        }
    }

    err = gated - s->y;
    aerr = err >= 0.0f ? err : -err;
    mix = aerr / (aerr + s->knee);
    st = s->st_max + (s->st_min - s->st_max) * mix;
    if (err < 0.0f) {
        st *= s->down_st;
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
