#include "smooth.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_MIN_CUTOFF 0.02f
#define DEFAULT_BETA       0.08f
#define DEFAULT_D_CUTOFF   1.0f
#define DEFAULT_EMA_ALPHA  0.25f
#define DEFAULT_AEMA_MIN   0.12f
#define DEFAULT_AEMA_MAX   0.85f
#define DEFAULT_AEMA_KNEE  4.0f

typedef enum {
    METHOD_PEAKCUT = 0,
    METHOD_ONEEURO,
    METHOD_AEMA,
    METHOD_EMA
} Method;

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [options] [file]\n"
            "\n"
            "Real-time peak-cut smoother. Each incoming sample produces one\n"
            "output immediately (no lookahead, O(1)). Connecting the outputs\n"
            "yields a spike-free smooth curve with low lag on real drops.\n"
            "\n"
            "Options:\n"
            "  --method peakcut|oneeuro|aema|ema\n"
            "                              Default: peakcut (streaming)\n"
            "  --radius R                  Medium-move confirm hold (default: %d).\n"
            "                              Must exceed the longest periodic tooth;\n"
            "                              large steps still confirm after 2 samples\n"
            "  --sigma S                   Transition roundness (default: %.1f).\n"
            "                              Larger = longer min-jerk horizon, rounder S-curves\n"
            "  --offline                   Batch peak-cut (uses future samples)\n"
            "  --live                      Read numbers from stdin as they arrive\n"
            "  --min-cutoff F --beta B --d-cutoff F --dt T\n"
            "                              1-Euro parameters\n"
            "  --alpha A                   Fixed EMA alpha\n"
            "  --min-alpha A --max-alpha A --knee K\n"
            "                              Adaptive EMA parameters\n"
            "  --csv                       Print n,raw,smoothed CSV only\n"
            "  --svg FILE                  Write a comparison SVG plot\n"
            "  --self-test                 Run numeric sanity checks and exit\n"
            "  -h, --help                  Show this help\n"
            "\n"
            "If FILE is omitted, samples.txt is used (fed sample-by-sample).\n"
            "Pipe live data:  ./smooth --live --csv\n",
            argv0, PEAKCUT_DEFAULT_RADIUS, PEAKCUT_DEFAULT_SIGMA);
}

static int parse_floats(const char *text, float **out, size_t *out_n)
{
    size_t cap = 64;
    size_t n = 0;
    float *v;
    const char *p = text;

    v = (float *)malloc(cap * sizeof(float));
    if (!v) {
        return -1;
    }

    while (*p) {
        char *end = NULL;
        float x;

        while (*p && !isdigit((unsigned char)*p) && *p != '-' && *p != '.') {
            p++;
        }
        if (!*p) {
            break;
        }

        errno = 0;
        x = strtof(p, &end);
        if (end == p || errno == ERANGE) {
            free(v);
            return -1;
        }

        if (n == cap) {
            float *nv;
            cap *= 2;
            nv = (float *)realloc(v, cap * sizeof(float));
            if (!nv) {
                free(v);
                return -1;
            }
            v = nv;
        }
        v[n++] = x;
        p = end;
    }

    *out = v;
    *out_n = n;
    return 0;
}

static char *read_file(const char *path, size_t *len_out)
{
    FILE *fp;
    long sz;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    sz = ftell(fp);
    if (sz < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (sz > 0 && fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(fp);
    if (len_out) {
        *len_out = (size_t)sz;
    }
    return buf;
}

static int load_samples(const char *path, float **out, size_t *n)
{
    char *text = read_file(path, NULL);
    int rc;

    if (!text) {
        fprintf(stderr, "failed to read %s: %s\n", path, strerror(errno));
        return -1;
    }
    rc = parse_floats(text, out, n);
    free(text);
    if (rc != 0 || *n == 0) {
        fprintf(stderr, "no numeric samples found in %s\n", path);
        return -1;
    }
    return 0;
}

static int run_filter(Method method, int offline, const float *x, float *y, size_t n,
                      int radius, float sigma,
                      float min_cutoff, float beta, float d_cutoff, float dt,
                      float ema_alpha, float aema_min, float aema_max,
                      float aema_knee)
{
    size_t i;
    OneEuro euro;
    AdaptiveEma aema;
    Ema ema;
    PeakCutStream stream;

    if (method == METHOD_PEAKCUT) {
        if (offline) {
            return peakcut_filter(x, y, n, radius, sigma);
        }
        peakcut_stream_init(&stream, radius, sigma);
        for (i = 0; i < n; i++) {
            y[i] = peakcut_stream_update(&stream, x[i]);
        }
        return 0;
    }

    one_euro_init(&euro, min_cutoff, beta, d_cutoff);
    aema_init(&aema, aema_min, aema_max, aema_knee);
    ema_init(&ema, ema_alpha);

    for (i = 0; i < n; i++) {
        switch (method) {
        case METHOD_ONEEURO:
            y[i] = one_euro_update(&euro, x[i], dt);
            break;
        case METHOD_AEMA:
            y[i] = aema_update(&aema, x[i]);
            break;
        case METHOD_EMA:
            y[i] = ema_update(&ema, x[i]);
            break;
        default:
            y[i] = x[i];
            break;
        }
    }
    return 0;
}

static float absf(float v)
{
    return v >= 0.0f ? v : -v;
}

static void print_stats(const float *x, const float *y, size_t n)
{
    size_t i;
    double sum_dx = 0.0;
    double sum_dy = 0.0;
    double max_dx = 0.0;
    double max_dy = 0.0;
    double max_d2 = 0.0;
    double raw_peak = 0.0;
    double sm_peak = 0.0;

    if (n == 0) {
        return;
    }

    for (i = 1; i < n; i++) {
        double dx = fabs((double)x[i] - (double)x[i - 1]);
        double dy = fabs((double)y[i] - (double)y[i - 1]);
        sum_dx += dx;
        sum_dy += dy;
        if (dx > max_dx) {
            max_dx = dx;
        }
        if (dy > max_dy) {
            max_dy = dy;
        }
        if (i >= 2) {
            double d2 = fabs((double)y[i] - 2.0 * (double)y[i - 1] + (double)y[i - 2]);
            if (d2 > max_d2) {
                max_d2 = d2;
            }
        }
    }

    for (i = 0; i < n; i++) {
        size_t a = (i > 8) ? i - 8 : 0;
        size_t b = i + 9;
        size_t k;
        double raw_mn;
        double sm_mn;
        if (b > n) {
            b = n;
        }
        raw_mn = x[a];
        sm_mn = y[a];
        for (k = a + 1; k < b; k++) {
            if (x[k] < raw_mn) {
                raw_mn = x[k];
            }
            if (y[k] < sm_mn) {
                sm_mn = y[k];
            }
        }
        if ((double)x[i] - raw_mn > raw_peak) {
            raw_peak = (double)x[i] - raw_mn;
        }
        if ((double)y[i] - sm_mn > sm_peak) {
            sm_peak = (double)y[i] - sm_mn;
        }
    }

    printf("samples          : %zu\n", n);
    printf("mean |dx| raw    : %.3f   max |dx|=%.2f\n",
           n > 1 ? sum_dx / (double)(n - 1) : 0.0, max_dx);
    printf("mean |dy| smooth : %.3f   max |dy|=%.3f\n",
           n > 1 ? sum_dy / (double)(n - 1) : 0.0, max_dy);
    printf("max curvature    : %.3f   (|y[i]-2y[i-1]+y[i-2]|)\n", max_d2);
    printf("local peak height: raw %.2f -> smooth %.2f  (over ±8 neighbors)\n",
           raw_peak, sm_peak);
}

static int write_svg(const char *path, const float *x, const float *y, size_t n)
{
    FILE *fp;
    size_t i;
    float xmin = 0.0f;
    float xmax;
    float ymin = x[0];
    float ymax = x[0];
    const int W = 960;
    const int H = 420;
    const int L = 48;
    const int R = 16;
    const int T = 28;
    const int B = 36;
    float pw;
    float ph;

    if (n == 0) {
        return -1;
    }

    xmax = n > 1 ? (float)(n - 1) : 1.0f;
    for (i = 0; i < n; i++) {
        if (x[i] < ymin) {
            ymin = x[i];
        }
        if (y[i] < ymin) {
            ymin = y[i];
        }
        if (x[i] > ymax) {
            ymax = x[i];
        }
        if (y[i] > ymax) {
            ymax = y[i];
        }
    }
    ymin -= 2.0f;
    ymax += 2.0f;
    if (ymax <= ymin) {
        ymax = ymin + 1.0f;
    }

    pw = (float)(W - L - R);
    ph = (float)(H - T - B);

    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "failed to write %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
            "viewBox=\"0 0 %d %d\">\n"
            "<rect width=\"100%%\" height=\"100%%\" fill=\"#0f1419\"/>\n"
            "<text x=\"%d\" y=\"20\" fill=\"#d7e0ea\" font-size=\"14\" "
            "font-family=\"sans-serif\">raw vs real-time peak-cut</text>\n"
            "<text x=\"%d\" y=\"20\" fill=\"#8aa0b4\" font-size=\"12\" "
            "font-family=\"sans-serif\" text-anchor=\"end\">"
            "<tspan fill=\"#6b7c8d\">raw</tspan>  "
            "<tspan fill=\"#5eead4\">smoothed</tspan></text>\n",
            W, H, W, H, L, W - R);

    for (i = 0; i < 5; i++) {
        float t = (float)i / 4.0f;
        float gy = (float)T + t * ph;
        float val = ymax - t * (ymax - ymin);
        fprintf(fp,
                "<line x1=\"%d\" y1=\"%.1f\" x2=\"%d\" y2=\"%.1f\" "
                "stroke=\"#243040\" stroke-width=\"1\"/>\n"
                "<text x=\"%d\" y=\"%.1f\" fill=\"#6b7c8d\" font-size=\"11\" "
                "font-family=\"sans-serif\" text-anchor=\"end\">%.0f</text>\n",
                L, gy, W - R, gy, L - 6, gy + 4.0f, val);
    }

    fprintf(fp, "<polyline fill=\"none\" stroke=\"#6b7c8d\" stroke-width=\"1.2\" "
                "stroke-opacity=\"0.9\" points=\"");
    for (i = 0; i < n; i++) {
        float px = (float)L + ((float)i - xmin) / (xmax - xmin) * pw;
        float py = (float)T + (ymax - x[i]) / (ymax - ymin) * ph;
        fprintf(fp, "%.1f,%.1f ", px, py);
    }
    fprintf(fp, "\"/>\n");

    fprintf(fp, "<polyline fill=\"none\" stroke=\"#5eead4\" stroke-width=\"2\" "
                "points=\"");
    for (i = 0; i < n; i++) {
        float px = (float)L + ((float)i - xmin) / (xmax - xmin) * pw;
        float py = (float)T + (ymax - y[i]) / (ymax - ymin) * ph;
        fprintf(fp, "%.1f,%.1f ", px, py);
    }
    fprintf(fp, "\"/>\n</svg>\n");
    fclose(fp);
    return 0;
}

static int nearly_equal(float a, float b, float eps)
{
    return absf(a - b) <= eps;
}

static int self_test(void)
{
    float spike[16];
    float spike_out[16];
    float ramp[40];
    float ramp_out[40];
    float y;
    int i;
    int fails = 0;
    Ema ema;

    for (i = 0; i < 16; i++) {
        spike[i] = 10.0f;
    }
    spike[8] = 90.0f;
    if (peakcut_filter(spike, spike_out, 16, 3, 1.0f) != 0) {
        fprintf(stderr, "FAIL: peakcut_filter returned error\n");
        return 1;
    }
    if (spike_out[8] > 14.0f) {
        fprintf(stderr, "FAIL: isolated spike not cut (%f)\n", spike_out[8]);
        fails++;
    }
    for (i = 0; i < 16; i++) {
        if (i == 8) {
            continue;
        }
        if (spike_out[i] > 13.0f) {
            fprintf(stderr, "FAIL: baseline warped at %d (%f)\n", i, spike_out[i]);
            fails++;
            break;
        }
    }

    for (i = 0; i < 40; i++) {
        ramp[i] = (float)i;
    }
    if (peakcut_filter(ramp, ramp_out, 40, 4, 1.5f) != 0) {
        fprintf(stderr, "FAIL: ramp peakcut_filter error\n");
        return 1;
    }
    /* A wide ramp is not a spike; the middle should still climb. */
    if (!(ramp_out[5] < ramp_out[20] && ramp_out[20] < ramp_out[35])) {
        fprintf(stderr, "FAIL: ramp was flattened (%f %f %f)\n",
                ramp_out[5], ramp_out[20], ramp_out[35]);
        fails++;
    }

    ema_init(&ema, 0.5f);
    (void)ema_update(&ema, 0.0f);
    y = ema_update(&ema, 10.0f);
    if (!nearly_equal(y, 5.0f, 1e-5f)) {
        fprintf(stderr, "FAIL: EMA alpha=0.5 expected 5, got %f\n", y);
        fails++;
    }

    {
        PeakCutStream st;
        float last;

        peakcut_stream_init(&st, 4, 2.0f);
        last = 0.0f;
        for (i = 0; i < 30; i++) {
            last = peakcut_stream_update(&st, 10.0f);
        }
        if (fabsf(last - 10.0f) > 0.05f) {
            fprintf(stderr, "FAIL: stream constant drifted (%f)\n", last);
            fails++;
        }

        peakcut_stream_init(&st, 4, 2.0f);
        for (i = 0; i < 12; i++) {
            last = peakcut_stream_update(&st, 10.0f);
        }
        (void)peakcut_stream_update(&st, 90.0f);
        for (i = 0; i < 8; i++) {
            last = peakcut_stream_update(&st, 10.0f);
        }
        if (last > 16.0f) {
            fprintf(stderr, "FAIL: stream did not cut spike (%f)\n", last);
            fails++;
        }

        peakcut_stream_init(&st, 3, 1.5f);
        last = peakcut_stream_update(&st, 0.0f);
        for (i = 1; i < 25; i++) {
            float yi = peakcut_stream_update(&st, (float)i);
            if (yi < last - 0.001f) {
                fprintf(stderr, "FAIL: stream ramp went backwards at %d (%f -> %f)\n",
                        i, last, yi);
                fails++;
                break;
            }
            last = yi;
        }
        if (last < 2.5f) {
            fprintf(stderr, "FAIL: stream ramp did not rise (%f)\n", last);
            fails++;
        }

        peakcut_stream_init(&st, 7, 5.0f);
        for (i = 0; i < 25; i++) {
            last = peakcut_stream_update(&st, 80.0f);
        }
        {
            float prev = last;
            float max_dy = 0.0f;
            for (i = 0; i < 100; i++) {
                float yi = peakcut_stream_update(&st, 30.0f);
                float dy = yi - prev;
                if (dy < 0.0f) {
                    dy = -dy;
                }
                if (dy > max_dy) {
                    max_dy = dy;
                }
                last = yi;
                prev = yi;
            }
            if (max_dy > 1.25f) {
                fprintf(stderr, "FAIL: stream drop too steep (max |dy|=%f)\n", max_dy);
                fails++;
            }
            if (last > 45.0f) {
                fprintf(stderr, "FAIL: stream drop did not follow (%f)\n", last);
                fails++;
            }
        }

        /* A large step must ease in: successive |dy| increases at the start. */
        peakcut_stream_init(&st, 16, 5.0f);
        for (i = 0; i < 20; i++) {
            last = peakcut_stream_update(&st, 10.0f);
        }
        {
            float prev = last;
            float dya[8];
            int ok = 1;
            for (i = 0; i < 8; i++) {
                last = peakcut_stream_update(&st, 90.0f);
                dya[i] = last - prev;
                prev = last;
            }
            /* Skip the 2-sample confirm; then slope must keep rising for a bit. */
            if (!(dya[3] > dya[2] + 0.004f && dya[4] > dya[3] + 0.004f &&
                  dya[5] > dya[4] + 0.004f)) {
                fprintf(stderr, "FAIL: step did not ease in (%.3f %.3f %.3f %.3f %.3f %.3f)\n",
                        dya[0], dya[1], dya[2], dya[3], dya[4], dya[5]);
                fails++;
                ok = 0;
            }
            for (i = 0; i < 8; i++) {
                if (dya[i] > 1.25f) {
                    fprintf(stderr, "FAIL: step too steep at %d (%f)\n", i, dya[i]);
                    fails++;
                    ok = 0;
                    break;
                }
            }
            (void)ok;
        }

        /* A long catch-up must not lock to a constant slope (straight ramp). */
        peakcut_stream_init(&st, 16, 5.0f);
        for (i = 0; i < 20; i++) {
            last = peakcut_stream_update(&st, 10.0f);
        }
        {
            float prev = last;
            int locked = 0;
            int run = 0;
            float prev_dy = 0.0f;
            for (i = 0; i < 160; i++) {
                float yi = peakcut_stream_update(&st, 90.0f);
                float dy = yi - prev;
                if (i > 4 && fabsf(dy) > 0.08f && fabsf(dy - prev_dy) < 1e-5f) {
                    run++;
                    if (run >= 6) {
                        locked = 1;
                    }
                } else {
                    run = 0;
                }
                prev_dy = dy;
                prev = yi;
                last = yi;
            }
            if (locked) {
                fprintf(stderr, "FAIL: step locked to a straight-line slope\n");
                fails++;
            }
            if (last < 70.0f) {
                fprintf(stderr, "FAIL: long step did not arrive (%f)\n", last);
                fails++;
            }
        }

        /* Two-sided gate: a 16-sample sawtooth on a plateau must not be followed. */
        peakcut_stream_init(&st, 16, 5.0f);
        for (i = 0; i < 40; i++) {
            last = peakcut_stream_update(&st, 88.0f);
        }
        {
            float plat_min = last;
            float plat_max = last;
            for (i = 0; i < 80; i++) {
                float tooth = (i % 16 < 8) ? 94.0f : 82.0f;
                last = peakcut_stream_update(&st, tooth);
                if (last < plat_min) {
                    plat_min = last;
                }
                if (last > plat_max) {
                    plat_max = last;
                }
            }
            if (plat_max - plat_min > 3.0f) {
                fprintf(stderr, "FAIL: plateau sawtooth not flattened (%f .. %f)\n",
                        plat_min, plat_max);
                fails++;
            }
        }
    }

    if (fails == 0) {
        printf("self-test: all checks passed\n");
        return 0;
    }
    printf("self-test: %d check(s) failed\n", fails);
    return 1;
}

int main(int argc, char **argv)
{
    const char *path = "samples.txt";
    const char *svg_path = NULL;
    Method method = METHOD_PEAKCUT;
    int radius = PEAKCUT_DEFAULT_RADIUS;
    float sigma = PEAKCUT_DEFAULT_SIGMA;
    float min_cutoff = DEFAULT_MIN_CUTOFF;
    float beta = DEFAULT_BETA;
    float d_cutoff = DEFAULT_D_CUTOFF;
    float dt = 1.0f;
    float ema_alpha = DEFAULT_EMA_ALPHA;
    float aema_min = DEFAULT_AEMA_MIN;
    float aema_max = DEFAULT_AEMA_MAX;
    float aema_knee = DEFAULT_AEMA_KNEE;
    int csv = 0;
    int offline = 0;
    int live = 0;
    int i;
    float *x = NULL;
    float *y = NULL;
    size_t n = 0;
    const char *method_name = "peakcut";

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            return self_test();
        }
        if (strcmp(argv[i], "--offline") == 0) {
            offline = 1;
            continue;
        }
        if (strcmp(argv[i], "--live") == 0) {
            live = 1;
            continue;
        }
        if (strcmp(argv[i], "--csv") == 0) {
            csv = 1;
            continue;
        }
        if (strcmp(argv[i], "--svg") == 0 && i + 1 < argc) {
            svg_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "peakcut") == 0) {
                method = METHOD_PEAKCUT;
                method_name = "peakcut";
            } else if (strcmp(argv[i], "oneeuro") == 0) {
                method = METHOD_ONEEURO;
                method_name = "oneeuro";
            } else if (strcmp(argv[i], "aema") == 0) {
                method = METHOD_AEMA;
                method_name = "aema";
            } else if (strcmp(argv[i], "ema") == 0) {
                method = METHOD_EMA;
                method_name = "ema";
            } else {
                fprintf(stderr, "unknown method: %s\n", argv[i]);
                return 2;
            }
            continue;
        }
        if (strcmp(argv[i], "--radius") == 0 && i + 1 < argc) {
            radius = (int)strtol(argv[++i], NULL, 10);
            continue;
        }
        if (strcmp(argv[i], "--sigma") == 0 && i + 1 < argc) {
            sigma = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--min-cutoff") == 0 && i + 1 < argc) {
            min_cutoff = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--beta") == 0 && i + 1 < argc) {
            beta = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--d-cutoff") == 0 && i + 1 < argc) {
            d_cutoff = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--dt") == 0 && i + 1 < argc) {
            dt = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--alpha") == 0 && i + 1 < argc) {
            ema_alpha = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--min-alpha") == 0 && i + 1 < argc) {
            aema_min = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--max-alpha") == 0 && i + 1 < argc) {
            aema_max = strtof(argv[++i], NULL);
            continue;
        }
        if (strcmp(argv[i], "--knee") == 0 && i + 1 < argc) {
            aema_knee = strtof(argv[++i], NULL);
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
        path = argv[i];
    }

    if (live) {
        PeakCutStream stream;
        float xv;
        float yv;
        int idx = 0;

        peakcut_stream_init(&stream, radius, sigma);
        if (csv) {
            printf("n,raw,smoothed\n");
        }
        while (scanf("%f", &xv) == 1) {
            yv = peakcut_stream_update(&stream, xv);
            if (csv) {
                printf("%d,%.4f,%.4f\n", idx, xv, yv);
            } else {
                printf("%3d  %8.3f  %8.3f\n", idx, xv, yv);
            }
            fflush(stdout);
            idx++;
        }
        return 0;
    }

    if (load_samples(path, &x, &n) != 0) {
        return 1;
    }
    y = (float *)malloc(n * sizeof(float));
    if (!y) {
        free(x);
        return 1;
    }

    if (run_filter(method, offline, x, y, n, radius, sigma, min_cutoff, beta,
                   d_cutoff, dt, ema_alpha, aema_min, aema_max, aema_knee) != 0) {
        fprintf(stderr, "filter failed\n");
        free(x);
        free(y);
        return 1;
    }

    if (csv) {
        printf("n,raw,smoothed\n");
        for (i = 0; i < (int)n; i++) {
            printf("%d,%.4f,%.4f\n", i, x[i], y[i]);
        }
    } else {
        printf("method           : %s%s\n", method_name, offline ? " (offline)" : " (realtime stream)");
        if (method == METHOD_PEAKCUT) {
            printf("peakcut params   : radius=%d  sigma=%.2f\n", radius, sigma);
        }
        print_stats(x, y, n);
        printf("\n  n   raw   smoothed\n");
        for (i = 0; i < (int)n; i++) {
            printf("%3d  %5.1f  %8.3f\n", i, x[i], y[i]);
        }
    }

    if (svg_path) {
        if (write_svg(svg_path, x, y, n) != 0) {
            free(x);
            free(y);
            return 1;
        }
        if (!csv) {
            printf("\nwrote %s\n", svg_path);
        }
    }

    free(x);
    free(y);
    return 0;
}
