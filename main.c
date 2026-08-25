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
    METHOD_ONEEURO = 0,
    METHOD_AEMA,
    METHOD_EMA
} Method;

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [options] [file]\n"
            "\n"
            "Real-time causal smoother. Each sample is processed in O(1) with\n"
            "no lookahead. Default method is the 1-Euro filter.\n"
            "\n"
            "Options:\n"
            "  --method oneeuro|aema|ema   Filter type (default: oneeuro)\n"
            "  --min-cutoff F              1-Euro min cutoff (default: %.2f)\n"
            "  --beta B                    1-Euro speed coefficient (default: %.2f)\n"
            "  --d-cutoff F                1-Euro derivative cutoff (default: %.2f)\n"
            "  --dt T                      Sample period for 1-Euro (default: 1)\n"
            "  --alpha A                   Fixed EMA alpha (default: %.2f)\n"
            "  --min-alpha A --max-alpha A --knee K\n"
            "                              Adaptive EMA parameters\n"
            "  --csv                       Print n,raw,smoothed CSV only\n"
            "  --svg FILE                  Write a comparison SVG plot\n"
            "  --self-test                 Run numeric sanity checks and exit\n"
            "  -h, --help                  Show this help\n"
            "\n"
            "If FILE is omitted, samples.txt in the current directory is used.\n",
            argv0, DEFAULT_MIN_CUTOFF, DEFAULT_BETA, DEFAULT_D_CUTOFF,
            DEFAULT_EMA_ALPHA);
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

static void run_filter(Method method, const float *x, float *y, size_t n,
                       float min_cutoff, float beta, float d_cutoff, float dt,
                       float ema_alpha, float aema_min, float aema_max,
                       float aema_knee)
{
    size_t i;
    OneEuro euro;
    AdaptiveEma aema;
    Ema ema;

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
        }
    }
}

static float absf(float v)
{
    return v >= 0.0f ? v : -v;
}

static void print_stats(const float *x, const float *y, size_t n)
{
    size_t i;
    double sum_abs = 0.0;
    double sum_dx = 0.0;
    double sum_dy = 0.0;
    double max_lag_drop = 0.0;
    size_t drop_at = 0;
    int found_drop = 0;

    if (n == 0) {
        return;
    }

    for (i = 0; i < n; i++) {
        sum_abs += fabs((double)y[i] - (double)x[i]);
        if (i > 0) {
            sum_dx += fabs((double)x[i] - (double)x[i - 1]);
            sum_dy += fabs((double)y[i] - (double)y[i - 1]);
        }
        if (!found_drop && i > 0 && x[i] < x[i - 1] - 8.0f) {
            found_drop = 1;
            drop_at = i;
        }
    }

    printf("samples          : %zu\n", n);
    printf("mean |y-x|       : %.3f   (tracking residual)\n", sum_abs / (double)n);
    printf("mean |dx| raw    : %.3f\n", n > 1 ? sum_dx / (double)(n - 1) : 0.0);
    printf("mean |dy| smooth : %.3f   (lower = less jitter)\n",
           n > 1 ? sum_dy / (double)(n - 1) : 0.0);

    if (found_drop) {
        for (i = drop_at; i < drop_at + 6 && i < n; i++) {
            double lag = fabs((double)y[i] - (double)x[i]);
            if (lag > max_lag_drop) {
                max_lag_drop = lag;
            }
        }
        printf("first steep drop : n=%zu  raw=%.1f->%.1f  smooth=%.2f  peak |y-x|=%.2f\n",
               drop_at, drop_at > 0 ? x[drop_at - 1] : x[drop_at], x[drop_at],
               y[drop_at], max_lag_drop);
    }
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
            "font-family=\"sans-serif\">raw vs real-time smooth</text>\n"
            "<text x=\"%d\" y=\"20\" fill=\"#8aa0b4\" font-size=\"12\" "
            "font-family=\"sans-serif\" text-anchor=\"end\">"
            "<tspan fill=\"#6b7c8d\">raw</tspan>  "
            "<tspan fill=\"#5eead4\">smoothed</tspan></text>\n",
            W, H, W, H, L, W - R);

    /* grid */
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
    OneEuro euro;
    AdaptiveEma aema;
    Ema ema;
    float y;
    int i;
    int fails = 0;

    one_euro_init(&euro, DEFAULT_MIN_CUTOFF, DEFAULT_BETA, DEFAULT_D_CUTOFF);
    y = one_euro_update(&euro, 10.0f, 1.0f);
    if (!nearly_equal(y, 10.0f, 1e-6f)) {
        fprintf(stderr, "FAIL: 1-Euro first sample should pass through\n");
        fails++;
    }
    for (i = 0; i < 40; i++) {
        y = one_euro_update(&euro, 10.0f, 1.0f);
    }
    if (!nearly_equal(y, 10.0f, 0.01f)) {
        fprintf(stderr, "FAIL: 1-Euro constant input drifted (%f)\n", y);
        fails++;
    }
    /* A step should be followed: after many samples y approaches 20. */
    for (i = 0; i < 80; i++) {
        y = one_euro_update(&euro, 20.0f, 1.0f);
    }
    if (y < 19.5f) {
        fprintf(stderr, "FAIL: 1-Euro step lag too large (%f)\n", y);
        fails++;
    }

    aema_init(&aema, 0.12f, 0.85f, 4.0f);
    (void)aema_update(&aema, 0.0f);
    for (i = 0; i < 30; i++) {
        y = aema_update(&aema, 50.0f);
    }
    if (y < 49.0f) {
        fprintf(stderr, "FAIL: adaptive EMA did not catch a large step (%f)\n", y);
        fails++;
    }

    ema_init(&ema, 0.5f);
    (void)ema_update(&ema, 0.0f);
    y = ema_update(&ema, 10.0f);
    if (!nearly_equal(y, 5.0f, 1e-5f)) {
        fprintf(stderr, "FAIL: EMA alpha=0.5 expected 5, got %f\n", y);
        fails++;
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
    Method method = METHOD_ONEEURO;
    float min_cutoff = DEFAULT_MIN_CUTOFF;
    float beta = DEFAULT_BETA;
    float d_cutoff = DEFAULT_D_CUTOFF;
    float dt = 1.0f;
    float ema_alpha = DEFAULT_EMA_ALPHA;
    float aema_min = DEFAULT_AEMA_MIN;
    float aema_max = DEFAULT_AEMA_MAX;
    float aema_knee = DEFAULT_AEMA_KNEE;
    int csv = 0;
    int i;
    float *x = NULL;
    float *y = NULL;
    size_t n = 0;
    const char *method_name = "oneeuro";

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            return self_test();
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
            if (strcmp(argv[i], "oneeuro") == 0) {
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

    if (load_samples(path, &x, &n) != 0) {
        return 1;
    }
    y = (float *)malloc(n * sizeof(float));
    if (!y) {
        free(x);
        return 1;
    }

    run_filter(method, x, y, n, min_cutoff, beta, d_cutoff, dt, ema_alpha,
               aema_min, aema_max, aema_knee);

    if (csv) {
        printf("n,raw,smoothed\n");
        for (i = 0; i < (int)n; i++) {
            printf("%d,%.4f,%.4f\n", i, x[i], y[i]);
        }
    } else {
        printf("method           : %s\n", method_name);
        if (method == METHOD_ONEEURO) {
            printf("1-Euro params    : min_cutoff=%.3f  beta=%.3f  d_cutoff=%.3f  dt=%.4f\n",
                   min_cutoff, beta, d_cutoff, dt);
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
