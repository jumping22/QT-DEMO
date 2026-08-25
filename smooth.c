#include "smooth.h"

#include <math.h>

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

/* Convert a cutoff (cycles per time unit) into an EMA blending factor. */
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
    /* mix -> 0 when still, -> 1 when |error| >> knee */
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
