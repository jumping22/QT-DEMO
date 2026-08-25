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
    /*
     * Rest-to-rest min-jerk always planned to STOP, so live ramps lagged
     * 50–70 samples. Horizon is now T = t_min + t_sqrt * sqrt(|e|)
     * and the endpoint velocity is dest_vel (EMA of Δdest).
     */
    s->t_min = 10.0f + 0.80f * sigma;
    if (s->t_min < 8.0f) {
        s->t_min = 8.0f;
    }
    if (s->t_min > 18.0f) {
        s->t_min = 18.0f;
    }
    s->t_sqrt = 3.4f + 0.22f * sigma;
    if (s->t_sqrt < 3.0f) {
        s->t_sqrt = 3.0f;
    }
    if (s->t_sqrt > 6.0f) {
        s->t_sqrt = 6.0f;
    }
    s->t_max = 28.0f + 2.4f * sigma;
    if (s->t_max < 32.0f) {
        s->t_max = 32.0f;
    }
    if (s->t_max > 56.0f) {
        s->t_max = 56.0f;
    }
    s->j_max = 0.072f - 0.0044f * sigma;
    if (s->j_max < 0.032f) {
        s->j_max = 0.032f;
    }
    if (s->j_max > 0.070f) {
        s->j_max = 0.070f;
    }
    s->v_beta = 0.30f;
    s->v_lim = 1.6f;
    /* Local range below this while close to y → leave sticky follow. */
    s->settle_span = 4.0f + 0.6f * sigma;
    s->settle_need = 6;
    /*
     * Unlock sticky on a reversal larger than a mid-move pause (~3)
     * but smaller than a periodic tooth (~10–14).
     */
    s->reverse_need = 4.5f + 0.5f * sigma;
    s->reverse_hold = 3;
    s->slow_alpha = 0.018f;
    s->trend_eps = 2.2f;
    s->cruise = 0.18f;
    s->ignore_up = 0.0f;
    s->ignore_dn = 0.0f;
}

/*
 * One sample of a 5th-order plan from (y, vel, acc) to
 * (dest, dest_vel, 0) in time T. dest_vel feedforward is what
 * cuts the lag of rest-to-rest min-jerk on live ramps.
 * trend_live keeps a small cruise velocity so a mid-move pause
 * does not brake the S-curve into a staircase platform.
 */
static float follow_minjerk(PeakCutStream *s, float dest, int trend_live)
{
    double y = (double)s->y;
    double v = (double)s->vel;
    double a = (double)s->acc;
    double e = (double)dest - y;
    double ae = e >= 0.0 ? e : -e;
    double vT;
    double T;
    double T2;
    double T3;
    double T4;
    double T5;
    double c3;
    double c4;
    double c5;
    double yn;
    double vn;
    double an;
    double j;
    double dd;
    float dv;

    dd = (double)dest - (double)s->dest_prev;
    if (s->sticky) {
        /* A confirm jumps dest; that is a step, not a velocity. */
        if (dd > 2.5 || dd < -2.5) {
            dv = 0.0f;
        } else {
            dv = s->dest_vel + s->v_beta * ((float)dd - s->dest_vel);
            if (dv > s->v_lim) {
                dv = s->v_lim;
            }
            if (dv < -s->v_lim) {
                dv = -s->v_lim;
            }
        }
        if (trend_live) {
            if (s->move_dir > 0 && dv < s->cruise) {
                dv = s->cruise;
            } else if (s->move_dir < 0 && dv > -s->cruise) {
                dv = -s->cruise;
            }
        }
        s->dest_vel = dv;
        vT = (double)dv;
    } else {
        s->dest_vel += s->v_beta * (0.0f - s->dest_vel);
        vT = 0.0;
    }
    s->dest_prev = dest;

    T = (double)s->t_min + (double)s->t_sqrt * sqrt(ae);
    if (T > (double)s->t_max) {
        T = (double)s->t_max;
    }
    if (T < 6.0) {
        T = 6.0;
    }

    T2 = T * T;
    T3 = T2 * T;
    T4 = T3 * T;
    T5 = T4 * T;
    c3 = (20.0 * e - (8.0 * vT + 12.0 * v) * T - 3.0 * a * T2) / (2.0 * T3);
    c4 = (-30.0 * e + (14.0 * vT + 16.0 * v) * T + 3.0 * a * T2) / (2.0 * T4);
    c5 = (12.0 * e - (6.0 * vT + 6.0 * v) * T - a * T2) / (2.0 * T5);
    yn = y + v + 0.5 * a + c3 + c4 + c5;
    vn = v + a + 3.0 * c3 + 4.0 * c4 + 5.0 * c5;
    an = a + 6.0 * c3 + 12.0 * c4 + 20.0 * c5;

    j = an - a;
    if (j > (double)s->j_max || j < -(double)s->j_max) {
        if (j > (double)s->j_max) {
            j = (double)s->j_max;
        } else {
            j = -(double)s->j_max;
        }
        an = a + j;
        vn = v + an;
        yn = y + vn;
    }

    s->y = (float)yn;
    s->vel = (float)vn;
    s->acc = (float)an;
    return s->y;
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

static void enter_sticky(PeakCutStream *s, float x, int move_dir)
{
    s->sticky = 1;
    s->up_count = 0;
    s->dn_count = 0;
    s->move_dir = move_dir;
    s->ext = x;
    s->reverse_count = 0;
    hist_reset(s);
    hist_push(s, x);
}

float peakcut_stream_update(PeakCutStream *s, float x)
{
    float gated;
    float d;
    float ad;
    int need;
    int confirmed;
    int trend_live;
    float dead_up;
    float dead_dn;
    float step_up;
    float step_dn;

    if (!s->initialized) {
        s->initialized = 1;
        s->y = x;
        s->vel = 0.0f;
        s->acc = 0.0f;
        s->dest_prev = x;
        s->dest_vel = 0.0f;
        s->x_slow = x;
        s->ext = x;
        s->move_dir = 0;
        s->reverse_count = 0;
        s->ignore_up = 0.0f;
        s->ignore_dn = 0.0f;
        s->up_count = 0;
        s->dn_count = 0;
        s->sticky = 0;
        hist_reset(s);
        return x;
    }

    d = x - s->y;
    ad = d >= 0.0f ? d : -d;
    s->x_slow += s->slow_alpha * (x - s->x_slow);
    trend_live = 0;
    {
        float td = x - s->x_slow;
        if (td < 0.0f) {
            td = -td;
        }
        if (td > s->trend_eps) {
            trend_live = 1;
        }
    }

    /*
     * Sticky follow: after a real move is confirmed, keep tracking x
     * until a reversal (periodic tooth) or a true long-term flat.
     * A same-direction pause in the middle of a rise/drop must not
     * unlock, or min-jerk brakes into a staircase platform.
     */
    if (s->sticky) {
        gated = x;
        hist_push(s, x);
        if (s->move_dir > 0) {
            if (x > s->ext) {
                s->ext = x;
                s->reverse_count = 0;
            } else if ((s->ext - x) >= s->reverse_need) {
                s->reverse_count++;
            } else {
                s->reverse_count = 0;
            }
        } else if (s->move_dir < 0) {
            if (x < s->ext) {
                s->ext = x;
                s->reverse_count = 0;
            } else if ((x - s->ext) >= s->reverse_need) {
                s->reverse_count++;
            } else {
                s->reverse_count = 0;
            }
        }
        if (hist_range(s) <= s->settle_span && ad <= s->dead && !trend_live) {
            s->settle_count++;
        } else {
            s->settle_count = 0;
        }
        if (s->reverse_count >= s->reverse_hold ||
            s->settle_count >= s->settle_need) {
            /* After cutting a tooth, widen the same-direction deadband
             * so the next period cannot 2-sample / hold-confirm as a new step. */
            if (s->reverse_count >= s->reverse_hold) {
                if (s->move_dir > 0 && s->ignore_up < s->big_up) {
                    s->ignore_up = s->big_up;
                } else if (s->move_dir < 0 && s->ignore_dn < s->big_dn) {
                    s->ignore_dn = s->big_dn;
                }
            }
            s->sticky = 0;
            s->move_dir = 0;
            s->reverse_count = 0;
            hist_reset(s);
            /* Freeze dest: do not chase the reversing tooth for one sample. */
            gated = s->y + s->leak * d;
            trend_live = 0;
        }
    } else {
        confirmed = 0;
        dead_up = s->dead;
        dead_dn = s->dead;
        if (s->ignore_up > dead_up) {
            dead_up = s->ignore_up;
        }
        if (s->ignore_dn > dead_dn) {
            dead_dn = s->ignore_dn;
        }
        step_up = s->big_up;
        step_dn = s->big_dn;
        if (s->ignore_up + 1.0f > step_up) {
            step_up = s->ignore_up + 1.0f;
        }
        if (s->ignore_dn + 1.0f > step_dn) {
            step_dn = s->ignore_dn + 1.0f;
        }
        if ((d > 0.0f && ad <= dead_up) || (d <= 0.0f && ad <= dead_dn)) {
            s->up_count = 0;
            s->dn_count = 0;
            gated = s->y + s->leak * d;
        } else if (d > 0.0f) {
            s->up_count++;
            s->dn_count = 0;
            need = (d >= step_up) ? 2 : s->hold;
            confirmed = s->up_count >= need;
            gated = confirmed ? x : (s->y + s->leak * d);
        } else {
            s->dn_count++;
            s->up_count = 0;
            need = ((-d) >= step_dn) ? 2 : s->hold;
            confirmed = s->dn_count >= need;
            gated = confirmed ? x : (s->y + s->leak * d);
        }
        if (confirmed) {
            enter_sticky(s, x, (d > 0.0f) ? 1 : -1);
            if (d > 0.0f) {
                s->ignore_dn = 0.0f;
            } else {
                s->ignore_up = 0.0f;
            }
        }
    }

    return follow_minjerk(s, gated, trend_live && s->sticky);
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
