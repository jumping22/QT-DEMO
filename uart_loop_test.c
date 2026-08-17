/*
 * RK3568 UART TX/RX test for /dev/ttyS9
 *
 * - Sends a 250-byte packet every 100 ms
 * - Each TX packet embeds a wall-clock timestamp
 * - RX runs in a dedicated thread
 * - TX/RX payloads are appended to log files
 *
 * Build (on board or with matching cross toolchain):
 *   make
 *   make CROSS_COMPILE=aarch64-linux-gnu-
 *
 * Usage:
 *   ./uart_loop_test [-d /dev/ttyS9] [-b 115200] [-t tx.log] [-r rx.log] [-n count]
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

#define PACKET_SIZE       250
#define DEFAULT_DEV       "/dev/ttyS9"
#define DEFAULT_BAUD      115200
#define DEFAULT_TX_LOG    "uart_tx.log"
#define DEFAULT_RX_LOG    "uart_rx.log"
#define TX_INTERVAL_MS    100
#define MAGIC0            0x55
#define MAGIC1            0xAA

#pragma pack(push, 1)
typedef struct {
    uint8_t  magic[2];      /* 0x55 0xAA */
    uint32_t seq;           /* little-endian sequence */
    int64_t  tv_sec;        /* wall-clock seconds */
    int64_t  tv_nsec;       /* wall-clock nanoseconds */
    uint16_t payload_len;   /* bytes after this header that are patterned data */
    /* remaining bytes to PACKET_SIZE filled with pattern + seq */
} pkt_header_t;
#pragma pack(pop)

static volatile sig_atomic_t g_running = 1;
static int g_fd = -1;

static void on_signal(int sig)
{
    (void)sig;
    g_running = 0;
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
    tio.c_cc[VTIME] = 1; /* 100 ms read timeout in canonical terms; with nonblock we poll */

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    /* Switch to blocking after configure for simpler TX; RX uses poll-style read */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    tcflush(fd, TCIOFLUSH);
    return fd;
}

static void fill_packet(uint8_t *buf, uint32_t seq)
{
    pkt_header_t *hdr;
    struct timespec ts;
    size_t i;
    size_t hdr_size = sizeof(pkt_header_t);

    memset(buf, 0, PACKET_SIZE);
    clock_gettime(CLOCK_REALTIME, &ts);

    hdr = (pkt_header_t *)buf;
    hdr->magic[0]    = MAGIC0;
    hdr->magic[1]    = MAGIC1;
    hdr->seq         = seq;
    hdr->tv_sec      = (int64_t)ts.tv_sec;
    hdr->tv_nsec     = (int64_t)ts.tv_nsec;
    hdr->payload_len = (uint16_t)(PACKET_SIZE - hdr_size);

    for (i = hdr_size; i < PACKET_SIZE; i++)
        buf[i] = (uint8_t)((seq + i) & 0xFF);
}

static void hex_dump_line(FILE *fp, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        fprintf(fp, "%02X%s", data[i], (i + 1 < len) ? " " : "");
    }
}

static void log_tx_packet(FILE *fp, uint32_t seq, const uint8_t *buf)
{
    const pkt_header_t *hdr = (const pkt_header_t *)buf;
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    fprintf(fp,
            "[TX] wall=%ld.%09ld seq=%u pkt_ts=%lld.%09lld bytes=%d hex=",
            (long)now.tv_sec, now.tv_nsec,
            seq,
            (long long)hdr->tv_sec, (long long)hdr->tv_nsec,
            PACKET_SIZE);
    hex_dump_line(fp, buf, PACKET_SIZE);
    fputc('\n', fp);
    fflush(fp);
}

static void log_rx_chunk(FILE *fp, const uint8_t *buf, size_t len)
{
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    fprintf(fp, "[RX] wall=%ld.%09ld len=%zu hex=",
            (long)now.tv_sec, now.tv_nsec, len);
    hex_dump_line(fp, buf, len);
    fputc('\n', fp);
    fflush(fp);
}

typedef struct {
    FILE *rx_fp;
} rx_arg_t;

static void *rx_thread(void *arg)
{
    rx_arg_t *ra = (rx_arg_t *)arg;
    uint8_t buf[512];
    ssize_t n;

    while (g_running) {
        n = read(g_fd, buf, sizeof(buf));
        if (n > 0) {
            log_rx_chunk(ra->rx_fp, buf, (size_t)n);
        } else if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            perror("read");
            break;
        } else {
            /* timeout / no data with VTIME; keep waiting */
            usleep(1000);
        }
    }
    return NULL;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [options]\n"
            "  -d <dev>     UART device (default %s)\n"
            "  -b <baud>    Baud rate (default %d)\n"
            "  -t <file>    TX log file (default %s)\n"
            "  -r <file>    RX log file (default %s)\n"
            "  -n <count>   Stop after N TX packets (0 = forever)\n"
            "  -h           Help\n",
            prog, DEFAULT_DEV, DEFAULT_BAUD, DEFAULT_TX_LOG, DEFAULT_RX_LOG);
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
    uint8_t packet[PACKET_SIZE];
    uint32_t seq = 0;
    struct timespec next;
    int rc = 0;

    while ((opt = getopt(argc, argv, "d:b:t:r:n:h")) != -1) {
        switch (opt) {
        case 'd':
            dev = optarg;
            break;
        case 'b':
            baud = atoi(optarg);
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

    if (sizeof(pkt_header_t) >= PACKET_SIZE) {
        fprintf(stderr, "internal error: header too large\n");
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    g_fd = open_uart(dev, baud);
    if (g_fd < 0)
        return 1;

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

    printf("UART test started: %s @ %d, packet=%d bytes, interval=%d ms\n",
           dev, baud, PACKET_SIZE, TX_INTERVAL_MS);
    printf("TX log: %s\nRX log: %s\nCtrl+C to stop.\n", tx_path, rx_path);

    clock_gettime(CLOCK_MONOTONIC, &next);

    while (g_running) {
        ssize_t wr;
        size_t off = 0;

        fill_packet(packet, seq);
        while (off < PACKET_SIZE && g_running) {
            wr = write(g_fd, packet + off, PACKET_SIZE - off);
            if (wr < 0) {
                if (errno == EINTR)
                    continue;
                perror("write");
                g_running = 0;
                rc = 1;
                break;
            }
            off += (size_t)wr;
        }

        if (off == PACKET_SIZE)
            log_tx_packet(tx_fp, seq, packet);

        seq++;
        if (max_pkts > 0 && seq >= max_pkts)
            break;

        next.tv_nsec += TX_INTERVAL_MS * 1000000L;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_sec += 1;
            next.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }

    g_running = 0;
    pthread_join(tid, NULL);
    printf("Stopped after %u TX packets.\n", seq);

out_rx:
    fclose(rx_fp);
out_tx:
    fclose(tx_fp);
out_fd:
    if (g_fd >= 0)
        close(g_fd);
    return rc;
}
