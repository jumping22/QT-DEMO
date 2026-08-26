#ifndef PRESSURE_FILTER_H
#define PRESSURE_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Real-time pressure smoother for MCU / FreeRTOS.
 *
 * Drop pressure_filter.h + pressure_filter.c into the project.
 * No malloc, no stdio, no OS calls. One input sample -> one output.
 *
 *   #include "pressure_filter.h"
 *
 *   static PressureFilter s_press;
 *
 *   void app_init(void)
 *   {
 *       pressure_filter_init(&s_press);
 *   }
 *
 *   void sensor_task(void *arg)
 *   {
 *       for (;;) {
 *           float raw = read_pressure();
 *           float filtered = pressure_filter_update(&s_press, raw);
 *           (void)filtered;
 *           vTaskDelay(pdMS_TO_TICKS(10));
 *       }
 *   }
 *
 * Use one PressureFilter per sensor. Do not call update() on the same
 * instance from two tasks/ISRs without a mutex. Link libm (-lm) for sqrtf.
 */

#define PRESSURE_FILTER_HIST_MAX   48
#define PRESSURE_FILTER_HOLD_DEF   16
#define PRESSURE_FILTER_SMOOTH_DEF 5.0f

typedef struct {
    int hold;
    float dead;
    float big_up;
    float big_dn;
    float leak;
    float t_min;
    float t_sqrt;
    float t_max;
    float j_max;
    float v_beta;
    float v_lim;
    float settle_span;
    int settle_need;
    float reverse_need;
    int reverse_hold;
    float slow_alpha;
    float trend_eps;
    float cruise;
    float ignore_up;
    float ignore_dn;
    float catch_on;
    float catch_ref;
    float lead_on;
    float lead_gain;
    float j_boost_on;
    float j_boost_ref;
    float j_boost_max;
    float v_lim_hi;
    float y;
    float vel;
    float acc;
    float dest_prev;
    float dest_vel;
    float x_slow;
    float ext;
    int move_dir;
    int reverse_count;
    float hist[PRESSURE_FILTER_HIST_MAX];
    int hist_len;
    int hist_pos;
    int sticky;
    int settle_count;
    int up_count;
    int dn_count;
    int initialized;
} PressureFilter;

/* Default hold=16, smoothness=5. Call once before update. */
void pressure_filter_init(PressureFilter *f);

/*
 * hold        : medium-move confirm length in samples (typical 8..24).
 * smoothness  : larger = rounder transitions, slightly more lag (0.8..16).
 */
void pressure_filter_init_ex(PressureFilter *f, int hold, float smoothness);

/* One raw pressure in, one smoothed pressure out. O(1), no lookahead. */
float pressure_filter_update(PressureFilter *f, float pressure);

/* Last smoothed value (0 before the first update). */
float pressure_filter_get(const PressureFilter *f);

#ifdef __cplusplus
}
#endif

#endif
