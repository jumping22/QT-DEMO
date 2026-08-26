#include "pressure_filter.h"

#include <math.h>
#include <string.h>

#define HOLD_MAX PRESSURE_FILTER_HIST_MAX

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

void pressure_filter_init(PressureFilter *f)
{
    pressure_filter_init_ex(f, PRESSURE_FILTER_HOLD_DEF, PRESSURE_FILTER_SMOOTH_DEF);
}

void pressure_filter_init_ex(PressureFilter *f, int hold, float smoothness)
{
    memset(f, 0, sizeof(*f));
    if (hold < 1) {
        hold = 1;
    }
    if (hold > HOLD_MAX) {
        hold = HOLD_MAX;
    }
    if (smoothness < 0.8f) {
        smoothness = 0.8f;
    }
    if (smoothness > 16.0f) {
        smoothness = 16.0f;
    }
    f->hold = hold;
    f->dead = 3.5f + 0.5f * smoothness;
    f->big_up = 8.0f + smoothness;
    f->big_dn = 6.0f + 0.8f * smoothness;
    f->leak = 0.06f / smoothness;
    if (f->leak > 0.035f) {
        f->leak = 0.035f;
    }
    if (f->leak < 0.010f) {
        f->leak = 0.010f;
    }
    f->t_min = clampf(10.0f + 0.80f * smoothness, 8.0f, 18.0f);
    f->t_sqrt = clampf(3.4f + 0.22f * smoothness, 3.0f, 6.0f);
    f->t_max = clampf(28.0f + 2.4f * smoothness, 32.0f, 56.0f);
    f->j_max = clampf(0.072f - 0.0044f * smoothness, 0.032f, 0.070f);
    f->v_beta = 0.30f;
    f->v_lim = 1.6f;
    f->settle_span = 4.0f + 0.6f * smoothness;
    f->settle_need = 6;
    f->reverse_need = 4.5f + 0.5f * smoothness;
    f->reverse_hold = 3;
    f->slow_alpha = 0.018f;
    f->trend_eps = 2.2f;
    f->cruise = 0.18f;
}

static void hist_reset(PressureFilter *f)
{
    f->hist_len = 0;
    f->hist_pos = 0;
    f->settle_count = 0;
}

static void hist_push(PressureFilter *f, float x)
{
    int cap = f->hold;

    if (cap < 1) {
        cap = 1;
    }
    if (cap > HOLD_MAX) {
        cap = HOLD_MAX;
    }
    f->hist[f->hist_pos] = x;
    f->hist_pos++;
    if (f->hist_pos >= cap) {
        f->hist_pos = 0;
    }
    if (f->hist_len < cap) {
        f->hist_len++;
    }
}

static float hist_range(const PressureFilter *f)
{
    int i;
    float mn;
    float mx;

    if (f->hist_len <= 0) {
        return 0.0f;
    }
    mn = f->hist[0];
    mx = f->hist[0];
    for (i = 1; i < f->hist_len; i++) {
        if (f->hist[i] < mn) {
            mn = f->hist[i];
        }
        if (f->hist[i] > mx) {
            mx = f->hist[i];
        }
    }
    return mx - mn;
}

static void enter_sticky(PressureFilter *f, float x, int move_dir)
{
    f->sticky = 1;
    f->up_count = 0;
    f->dn_count = 0;
    f->move_dir = move_dir;
    f->ext = x;
    f->reverse_count = 0;
    hist_reset(f);
    hist_push(f, x);
}

static float follow_minjerk(PressureFilter *f, float dest, int trend_live)
{
    float y = f->y;
    float v = f->vel;
    float a = f->acc;
    float e = dest - y;
    float ae = (e >= 0.0f) ? e : -e;
    float vT;
    float T;
    float T2;
    float T3;
    float T4;
    float T5;
    float c3;
    float c4;
    float c5;
    float yn;
    float vn;
    float an;
    float j;
    float dd;
    float dv;

    dd = dest - f->dest_prev;
    if (f->sticky) {
        if (dd > 2.5f || dd < -2.5f) {
            dv = 0.0f;
        } else {
            dv = f->dest_vel + f->v_beta * (dd - f->dest_vel);
            dv = clampf(dv, -f->v_lim, f->v_lim);
        }
        if (trend_live) {
            if (f->move_dir > 0 && dv < f->cruise) {
                dv = f->cruise;
            } else if (f->move_dir < 0 && dv > -f->cruise) {
                dv = -f->cruise;
            }
        }
        f->dest_vel = dv;
        vT = dv;
    } else {
        f->dest_vel += f->v_beta * (0.0f - f->dest_vel);
        vT = 0.0f;
    }
    f->dest_prev = dest;

    T = f->t_min + f->t_sqrt * sqrtf(ae);
    if (T > f->t_max) {
        T = f->t_max;
    }
    if (T < 6.0f) {
        T = 6.0f;
    }

    T2 = T * T;
    T3 = T2 * T;
    T4 = T3 * T;
    T5 = T4 * T;
    c3 = (20.0f * e - (8.0f * vT + 12.0f * v) * T - 3.0f * a * T2) / (2.0f * T3);
    c4 = (-30.0f * e + (14.0f * vT + 16.0f * v) * T + 3.0f * a * T2) / (2.0f * T4);
    c5 = (12.0f * e - (6.0f * vT + 6.0f * v) * T - a * T2) / (2.0f * T5);
    yn = y + v + 0.5f * a + c3 + c4 + c5;
    vn = v + a + 3.0f * c3 + 4.0f * c4 + 5.0f * c5;
    an = a + 6.0f * c3 + 12.0f * c4 + 20.0f * c5;

    j = an - a;
    if (j > f->j_max || j < -f->j_max) {
        if (j > f->j_max) {
            j = f->j_max;
        } else {
            j = -f->j_max;
        }
        an = a + j;
        vn = v + an;
        yn = y + vn;
    }

    f->y = yn;
    f->vel = vn;
    f->acc = an;
    return f->y;
}

float pressure_filter_update(PressureFilter *f, float pressure)
{
    float x = pressure;
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

    if (!f->initialized) {
        f->initialized = 1;
        f->y = x;
        f->vel = 0.0f;
        f->acc = 0.0f;
        f->dest_prev = x;
        f->dest_vel = 0.0f;
        f->x_slow = x;
        f->ext = x;
        f->move_dir = 0;
        f->reverse_count = 0;
        f->ignore_up = 0.0f;
        f->ignore_dn = 0.0f;
        f->up_count = 0;
        f->dn_count = 0;
        f->sticky = 0;
        hist_reset(f);
        return x;
    }

    d = x - f->y;
    ad = (d >= 0.0f) ? d : -d;
    f->x_slow += f->slow_alpha * (x - f->x_slow);
    trend_live = 0;
    {
        float td = x - f->x_slow;
        if (td < 0.0f) {
            td = -td;
        }
        if (td > f->trend_eps) {
            trend_live = 1;
        }
    }

    if (f->sticky) {
        gated = x;
        hist_push(f, x);
        if (f->move_dir > 0) {
            if (x > f->ext) {
                f->ext = x;
                f->reverse_count = 0;
            } else if ((f->ext - x) >= f->reverse_need) {
                f->reverse_count++;
            } else {
                f->reverse_count = 0;
            }
        } else if (f->move_dir < 0) {
            if (x < f->ext) {
                f->ext = x;
                f->reverse_count = 0;
            } else if ((x - f->ext) >= f->reverse_need) {
                f->reverse_count++;
            } else {
                f->reverse_count = 0;
            }
        }
        if (hist_range(f) <= f->settle_span && ad <= f->dead && !trend_live) {
            f->settle_count++;
        } else {
            f->settle_count = 0;
        }
        if (f->reverse_count >= f->reverse_hold ||
            f->settle_count >= f->settle_need) {
            if (f->reverse_count >= f->reverse_hold) {
                if (f->move_dir > 0 && f->ignore_up < f->big_up) {
                    f->ignore_up = f->big_up;
                } else if (f->move_dir < 0 && f->ignore_dn < f->big_dn) {
                    f->ignore_dn = f->big_dn;
                }
            }
            f->sticky = 0;
            f->move_dir = 0;
            f->reverse_count = 0;
            hist_reset(f);
            gated = f->y + f->leak * d;
            trend_live = 0;
        }
    } else {
        confirmed = 0;
        dead_up = f->dead;
        dead_dn = f->dead;
        if (f->ignore_up > dead_up) {
            dead_up = f->ignore_up;
        }
        if (f->ignore_dn > dead_dn) {
            dead_dn = f->ignore_dn;
        }
        step_up = f->big_up;
        step_dn = f->big_dn;
        if (f->ignore_up + 1.0f > step_up) {
            step_up = f->ignore_up + 1.0f;
        }
        if (f->ignore_dn + 1.0f > step_dn) {
            step_dn = f->ignore_dn + 1.0f;
        }
        if ((d > 0.0f && ad <= dead_up) || (d <= 0.0f && ad <= dead_dn)) {
            f->up_count = 0;
            f->dn_count = 0;
            gated = f->y + f->leak * d;
        } else if (d > 0.0f) {
            f->up_count++;
            f->dn_count = 0;
            need = (d >= step_up) ? 2 : f->hold;
            confirmed = f->up_count >= need;
            gated = confirmed ? x : (f->y + f->leak * d);
        } else {
            f->dn_count++;
            f->up_count = 0;
            need = ((-d) >= step_dn) ? 2 : f->hold;
            confirmed = f->dn_count >= need;
            gated = confirmed ? x : (f->y + f->leak * d);
        }
        if (confirmed) {
            enter_sticky(f, x, (d > 0.0f) ? 1 : -1);
            if (d > 0.0f) {
                f->ignore_dn = 0.0f;
            } else {
                f->ignore_up = 0.0f;
            }
        }
    }

    return follow_minjerk(f, gated, trend_live && f->sticky);
}

float pressure_filter_get(const PressureFilter *f)
{
    return f->y;
}
