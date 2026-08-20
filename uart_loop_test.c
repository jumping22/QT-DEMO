/*
 * RK3568 UART loopback test for /dev/ttyS9
 *
 * Self-loop: TX is wired back to RX on the same port.
 * - Sends packets at a configurable interval (default 100 ms)
 * - Packet size is configurable (default 250 bytes; embeds wall-clock timestamp)
 * - RX thread reassembles frames and validates them
 * - TX/RX payloads are appended to log files
 * - On exit, prints packet loss / error statistics
 *
 * Build:
 *   make
 *   make CROSS_COMPILE=aarch64-linux-gnu-
 *
 * Usage:
 *   ./uart_loop_test [-d /dev/ttyS9] [-b 115200] [-s 250] [-i 100]
 *                    [-t tx.log] [-r rx.log] [-n count]
 *   Ctrl+C to stop
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PACKET_SIZE 250
#define DEFAULT_INTERVAL_MS 100
#define DEFAULT_DEV         "/dev/ttyS9"
#define DEFAULT_BAUD        115200
#define DEFAULT_TX_LOG      "/tmp/uart_tx.log"
#define DEFAULT_RX_LOG      "/tmp/uart_rx.log"
#define RX_DRAIN_MS         500
#define MAX_PACKET_SIZE     8192
#define MAGIC0              0x55
#define MAGIC1              0xAA
#define PROG_VERSION        "1.2.0-loopback"

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[2];      /* 0x55 0xAA */
    uint32_t seq;           /* little-endian sequence */
    int64_t  tv_sec;        /* wall-clock seconds */
    int64_t  tv_nsec;       /* wall-clock nanoseconds */
    uint16_t payload_len;   /* patterned payload bytes after header */
} pkt_header_t;
#pragma pack(pop)

#define HDR_SIZE            ((int)sizeof(pkt_header_t))
#define MIN_PACKET_SIZE     (HDR_SIZE + 1)

typedef struct {
    uint8_t *bits;
    size_t   bytes;         /* capacity in bytes */
    uint32_t count;         /* number of unique seqs marked */
} seq_set_t;

typedef struct {
    pthread_mutex_t lock;
    uint64_t tx_ok;
    uint64_t rx_ok;
    uint64_t rx_dup;
    uint64_t rx_bad;
    uint64_t rx_bytes;
    uint32_t tx_max_seq;    /* last successfully sent seq + 1 (== tx count) */
    seq_set_t rx_seen;
} stats_t;

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_tx_done = 0;
static int g_fd = -1;
static stats_t g_stats;
static int g_packet_size = DEFAULT_PACKET_SIZE;
static int g_interval_ms = DEFAULT_INTERVAL_MS;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static int seq_set_ensure(seq_set_t *set, uint32_t seq)
{
    size_t need = (size_t)(seq / 8) + 1;
    size_t ncap;
    uint8_t *nb;

    if (need <= set->bytes)
        return 0;

    ncap = set->bytes ? set->bytes : 64;
    while (ncap < need)
        ncap *= 2;

    nb = (uint8_t *)realloc(set->bits, ncap);
    if (!nb)
        return -1;
    memset(nb + set->bytes, 0, ncap - set->bytes);
    set->bits = nb;
    set->bytes = ncap;
    return 0;
}

/* returns 1 if newly marked, 0 if already present, -1 on OOM */
static int seq_set_mark(seq_set_t *set, uint32_t seq)
{
    size_t idx = (size_t)(seq / 8);
    uint8_t mask = (uint8_t)(1u << (seq % 8));

    if (seq_set_ensure(set, seq) != 0)
        return -1;
    if (set->bits[idx] & mask)
        return 0;
    set->bits[idx] |= mask;
    set->count++;
    return 1;
}

static int seq_set_test(const seq_set_t *set, uint32_t seq)
{
    size_t idx = (size_t)(seq / 8);
    uint8_t mask = (uint8_t)(1u << (seq % 8));

    if (idx >= set->bytes)
        return 0;
    return (set->bits[idx] & mask) != 0;
}

static void seq_set_free(seq_set_t *set)
{
    free(set->bits);
    set->bits = NULL;
    set->bytes = 0;
    set->count = 0;
}

static speed_t baud_to_flag(int baud)
{
    switch (baud) {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    default:      return (speed_t)-1;
    }
}

static int open_uart(const char *dev, int baud)
{
    int fd;
    struct termios tio;
    speed_t speed;

    speed = baud_to_flag(baud);
    if (speed == (speed_t)-1) {
        fprintf(stderr, "unsupported baud rate: %d\n", baud);
        return -1;
    }

    fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror(dev);
        return -1;
    }

    if (tcgetattr(fd, &tio) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfmakeraw(&tio);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CRTSCTS;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    /* Keep O_NONBLOCK: some RK UART drivers ignore VTIME and would block forever. */
    tcflush(fd, TCIOFLUSH);
    return fd;
}

static void fill_packet(uint8_t *buf, uint32_t seq)
{
    pkt_header_t *hdr;
    struct timespec ts;
    size_t i;
    int pkt_size = g_packet_size;

    memset(buf, 0, (size_t)pkt_size);
    clock_gettime(CLOCK_REALTIME, &ts);

    hdr = (pkt_header_t *)buf;
    hdr->magic[0]    = MAGIC0;
    hdr->magic[1]    = MAGIC1;
    hdr->seq         = seq;
    hdr->tv_sec      = (int64_t)ts.tv_sec;
    hdr->tv_nsec     = (int64_t)ts.tv_nsec;
    hdr->payload_len = (uint16_t)(pkt_size - HDR_SIZE);

    for (i = (size_t)HDR_SIZE; i < (size_t)pkt_size; i++)
        buf[i] = (uint8_t)((seq + i) & 0xFF);
}

static int validate_packet(const uint8_t *buf)
{
    const pkt_header_t *hdr = (const pkt_header_t *)buf;
    size_t i;
    int pkt_size = g_packet_size;

    if (hdr->magic[0] != MAGIC0 || hdr->magic[1] != MAGIC1)
        return 0;
    if (hdr->payload_len != (uint16_t)(pkt_size - HDR_SIZE))
        return 0;
    if (hdr->tv_nsec < 0 || hdr->tv_nsec >= 1000000000LL)
        return 0;

    for (i = (size_t)HDR_SIZE; i < (size_t)pkt_size; i++) {
        if (buf[i] != (uint8_t)((hdr->seq + i) & 0xFF))
            return 0;
    }
    return 1;
}

static void hex_dump_line(FILE *fp, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        fprintf(fp, "%02X%s", data[i], (i + 1 < len) ? " " : "");
}

static void log_tx_packet(FILE *fp, uint32_t seq, const uint8_t *buf)
{
    const pkt_header_t *hdr = (const pkt_header_t *)buf;
    struct timespec now;
    int pkt_size = g_packet_size;

    clock_gettime(CLOCK_REALTIME, &now);
    fprintf(fp,
            "[TX] wall=%ld.%09ld seq=%u pkt_ts=%lld.%09lld bytes=%d hex=",
            (long)now.tv_sec, now.tv_nsec,
            seq,
            (long long)hdr->tv_sec, (long long)hdr->tv_nsec,
            pkt_size);
    hex_dump_line(fp, buf, (size_t)pkt_size);
    fputc('\n', fp);
    fflush(fp);
}

static void log_rx_packet(FILE *fp, const uint8_t *buf, int ok, int is_dup)
{
    const pkt_header_t *hdr = (const pkt_header_t *)buf;
    struct timespec now;
    int pkt_size = g_packet_size;

    clock_gettime(CLOCK_REALTIME, &now);
    fprintf(fp,
            "[RX] wall=%ld.%09ld seq=%u ok=%d dup=%d pkt_ts=%lld.%09lld bytes=%d hex=",
            (long)now.tv_sec, now.tv_nsec,
            hdr->seq, ok, is_dup,
            (long long)hdr->tv_sec, (long long)hdr->tv_nsec,
            pkt_size);
    hex_dump_line(fp, buf, (size_t)pkt_size);
    fputc('\n', fp);
    fflush(fp);
}

static void handle_rx_frame(FILE *rx_fp, const uint8_t *frame)
{
    const pkt_header_t *hdr = (const pkt_header_t *)frame;
    int ok = validate_packet(frame);
    int marked;
    int is_dup = 0;

    pthread_mutex_lock(&g_stats.lock);
    if (!ok) {
        g_stats.rx_bad++;
        pthread_mutex_unlock(&g_stats.lock);
        log_rx_packet(rx_fp, frame, 0, 0);
        return;
    }

    marked = seq_set_mark(&g_stats.rx_seen, hdr->seq);
    if (marked < 0) {
        pthread_mutex_unlock(&g_stats.lock);
        fprintf(stderr, "oom while tracking RX seq\n");
        g_running = 0;
        return;
    }
    if (marked == 0) {
        g_stats.rx_dup++;
        is_dup = 1;
    } else {
        g_stats.rx_ok++;
    }
    pthread_mutex_unlock(&g_stats.lock);

    log_rx_packet(rx_fp, frame, 1, is_dup);
}

/*
 * Feed raw UART bytes into a stream reassembler.
 * Looks for 0x55 0xAA sync, then consumes g_packet_size bytes per frame.
 */
static void rx_feed(FILE *rx_fp, uint8_t *acc, size_t *acc_len,
                    const uint8_t *data, size_t len)
{
    size_t i = 0;
    size_t pkt_size = (size_t)g_packet_size;

    while (i < len) {
        if (*acc_len == 0) {
            while (i < len && data[i] != MAGIC0)
                i++;
            if (i >= len)
                break;
            acc[(*acc_len)++] = data[i++];
            continue;
        }

        if (*acc_len == 1) {
            if (data[i] != MAGIC1) {
                *acc_len = 0;
                continue;
            }
            acc[(*acc_len)++] = data[i++];
            continue;
        }

        {
            size_t need = pkt_size - *acc_len;
            size_t take = len - i;
            if (take > need)
                take = need;
            memcpy(acc + *acc_len, data + i, take);
            *acc_len += take;
            i += take;
        }

        if (*acc_len == pkt_size) {
            handle_rx_frame(rx_fp, acc);
            *acc_len = 0;
        }
    }
}

typedef struct {
    FILE *rx_fp;
} rx_arg_t;

static void set_fd_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void *rx_thread(void *arg)
{
    rx_arg_t *ra = (rx_arg_t *)arg;
    uint8_t buf[512];
    uint8_t *acc = NULL;
    size_t acc_len = 0;
    ssize_t n;
    struct timespec drain_start;
    int drain_timing = 0;

    acc = (uint8_t *)malloc((size_t)g_packet_size);
    if (!acc) {
        fprintf(stderr, "oom allocating RX accumulator\n");
        g_running = 0;
        return NULL;
    }

    while (1) {
        n = read(g_fd, buf, sizeof(buf));
        if (n > 0) {
            pthread_mutex_lock(&g_stats.lock);
            g_stats.rx_bytes += (uint64_t)n;
            pthread_mutex_unlock(&g_stats.lock);
            rx_feed(ra->rx_fp, acc, &acc_len, buf, (size_t)n);
            drain_timing = 0;
        } else if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(2000);
            } else {
                perror("read");
                break;
            }
        } else {
            usleep(2000);
        }

        if (!g_running && g_tx_done) {
            struct timespec now;
            long elapsed_ms;

            if (!drain_timing) {
                clock_gettime(CLOCK_MONOTONIC, &drain_start);
                drain_timing = 1;
            }
            clock_gettime(CLOCK_MONOTONIC, &now);
            elapsed_ms = (now.tv_sec - drain_start.tv_sec) * 1000L +
                         (now.tv_nsec - drain_start.tv_nsec) / 1000000L;
            if (elapsed_ms >= RX_DRAIN_MS)
                break;
        } else if (!g_running && !g_tx_done) {
            usleep(1000);
        }
    }

    free(acc);
    return NULL;
}

static void print_loss_report(void)
{
    uint64_t tx_ok;
    uint64_t rx_ok;
    uint64_t rx_dup;
    uint64_t rx_bad;
    uint64_t rx_bytes;
    uint32_t tx_max_seq;
    uint32_t unique_rx;
    uint64_t lost = 0;
    uint32_t i;
    double loss_pct = 0.0;

    pthread_mutex_lock(&g_stats.lock);
    tx_ok = g_stats.tx_ok;
    rx_ok = g_stats.rx_ok;
    rx_dup = g_stats.rx_dup;
    rx_bad = g_stats.rx_bad;
    rx_bytes = g_stats.rx_bytes;
    tx_max_seq = g_stats.tx_max_seq;
    unique_rx = g_stats.rx_seen.count;

    for (i = 0; i < tx_max_seq; i++) {
        if (!seq_set_test(&g_stats.rx_seen, i))
            lost++;
    }
    pthread_mutex_unlock(&g_stats.lock);

    if (tx_ok > 0)
        loss_pct = (100.0 * (double)lost) / (double)tx_ok;

    printf("\n======== Loopback result ========\n");
    printf("Packet size    : %d bytes\n", g_packet_size);
    printf("TX interval    : %d ms\n", g_interval_ms);
    printf("TX packets     : %llu\n", (unsigned long long)tx_ok);
    printf("RX unique OK   : %llu (tracked unique=%u)\n",
           (unsigned long long)rx_ok, unique_rx);
    printf("RX duplicates  : %llu\n", (unsigned long long)rx_dup);
    printf("RX bad frames  : %llu\n", (unsigned long long)rx_bad);
    printf("RX raw bytes   : %llu\n", (unsigned long long)rx_bytes);
    printf("Lost packets   : %llu\n", (unsigned long long)lost);
    printf("Loss rate      : %.4f%% (%llu / %llu)\n",
           loss_pct,
           (unsigned long long)lost,
           (unsigned long long)tx_ok);
    printf("=================================\n");
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  UART self-loopback test: TX is expected to loop into RX.\n"
            "  -d <dev>     UART device (default %s)\n"
            "  -b <baud>    Baud rate (default %d)\n"
            "  -s <bytes>   Packet size in bytes (default %d, min %d, max %d)\n"
            "  -i <ms>      TX interval in milliseconds (default %d, min 1)\n"
            "  -t <file>    TX log file (default %s)\n"
            "  -r <file>    RX log file (default %s)\n"
            "  -n <count>   Stop after N TX packets (0 = forever)\n"
            "  -h           Help\n",
            prog, DEFAULT_DEV, DEFAULT_BAUD,
            DEFAULT_PACKET_SIZE, MIN_PACKET_SIZE, MAX_PACKET_SIZE,
            DEFAULT_INTERVAL_MS,
            DEFAULT_TX_LOG, DEFAULT_RX_LOG);
}

static int parse_positive_int(const char *s, const char *name, int min_v, int max_v, int *out)
{
    char *end = NULL;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') {
        fprintf(stderr, "invalid %s: %s\n", name, s);
        return -1;
    }
    if (v < min_v || v > max_v) {
        fprintf(stderr, "%s out of range [%d, %d]: %ld\n", name, min_v, max_v, v);
        return -1;
    }
    *out = (int)v;
    return 0;
}

int main(int argc, char **argv)
{
    const char *dev = DEFAULT_DEV;
    const char *tx_path = DEFAULT_TX_LOG;
    const char *rx_path = DEFAULT_RX_LOG;
    int baud = DEFAULT_BAUD;
    unsigned long max_pkts = 0;
    int opt;
    FILE *tx_fp = NULL;
    FILE *rx_fp = NULL;
    pthread_t tid;
    rx_arg_t ra;
    uint8_t *packet = NULL;
    uint32_t seq = 0;
    struct timespec next;
    int rc = 0;
    int rx_started = 0;

    while ((opt = getopt(argc, argv, "d:b:s:i:t:r:n:h")) != -1) {
        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'b':
            baud = atoi(optarg);
            break;
        case 's':
            if (parse_positive_int(optarg, "packet size (-s)",
                                   MIN_PACKET_SIZE, MAX_PACKET_SIZE,
                                   &g_packet_size) != 0)
                return 1;
            break;
        case 'i':
            if (parse_positive_int(optarg, "TX interval (-i)",
                                   1, 3600000, &g_interval_ms) != 0)
                return 1;
            break;
        case 't':
            tx_path = optarg;
            break;
        case 'r':
            rx_path = optarg;
            break;
        case 'n':
            max_pkts = strtoul(optarg, NULL, 10);
            break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    if (g_packet_size <= HDR_SIZE) {
        fprintf(stderr, "packet size must be > header (%d)\n", HDR_SIZE);
        return 1;
    }

    packet = (uint8_t *)malloc((size_t)g_packet_size);
    if (!packet) {
        fprintf(stderr, "oom allocating TX packet buffer\n");
        return 1;
    }

    memset(&g_stats, 0, sizeof(g_stats));
    pthread_mutex_init(&g_stats.lock, NULL);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    g_fd = open_uart(dev, baud);
    if (g_fd < 0) {
        rc = 1;
        goto out_packet;
    }

    tx_fp = fopen(tx_path, "ab");
    if (!tx_fp) {
        perror(tx_path);
        rc = 1;
        goto out_fd;
    }

    rx_fp = fopen(rx_path, "ab");
    if (!rx_fp) {
        perror(rx_path);
        rc = 1;
        goto out_tx;
    }

    ra.rx_fp = rx_fp;
    if (pthread_create(&tid, NULL, rx_thread, &ra) != 0) {
        perror("pthread_create");
        rc = 1;
        goto out_rx;
    }
    rx_started = 1;

    printf("uart_loop_test %s\n", PROG_VERSION);
    printf("UART loopback test: %s @ %d, packet=%d bytes, interval=%d ms\n",
           dev, baud, g_packet_size, g_interval_ms);
    printf("TX log: %s\nRX log: %s\nCtrl+C to stop; loss rate printed on exit.\n",
           tx_path, rx_path);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &next);

    while (g_running) {
        ssize_t wr;
        size_t off = 0;
        size_t pkt_size = (size_t)g_packet_size;

        fill_packet(packet, seq);
        while (off < pkt_size && g_running) {
            wr = write(g_fd, packet + off, pkt_size - off);
            if (wr < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(1000);
                    continue;
                }
                perror("write");
                g_running = 0;
                rc = 1;
                break;
            }
            off += (size_t)wr;
        }

        if (off == pkt_size) {
            log_tx_packet(tx_fp, seq, packet);
            pthread_mutex_lock(&g_stats.lock);
            g_stats.tx_ok++;
            g_stats.tx_max_seq = seq + 1;
            pthread_mutex_unlock(&g_stats.lock);
            seq++;
        }

        if (max_pkts > 0 && seq >= max_pkts)
            break;

        next.tv_nsec += (long)g_interval_ms * 1000000L;
        while (next.tv_nsec >= 1000000000L) {
            next.tv_sec += 1;
            next.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    printf("Stopping after %u TX packets, draining RX...\n", seq);
    fflush(stdout);
    g_tx_done = 1;
    g_running = 0;
    set_fd_nonblock(g_fd);
    pthread_join(tid, NULL);
    rx_started = 0;

    print_loss_report();
    fflush(stdout);
    if (g_stats.tx_ok > 0) {
        uint64_t lost = 0;
        uint32_t i;
        for (i = 0; i < g_stats.tx_max_seq; i++) {
            if (!seq_set_test(&g_stats.rx_seen, i))
                lost++;
        }
        if (lost > 0 || g_stats.rx_bad > 0)
            rc = 2;
    }

out_rx:
    if (rx_started) {
        g_tx_done = 1;
        g_running = 0;
        set_fd_nonblock(g_fd);
        pthread_join(tid, NULL);
    }
    fclose(rx_fp);
out_tx:
    fclose(tx_fp);
out_fd:
    if (g_fd >= 0)
        close(g_fd);
out_packet:
    free(packet);
    seq_set_free(&g_stats.rx_seen);
    pthread_mutex_destroy(&g_stats.lock);
    return rc;
}
