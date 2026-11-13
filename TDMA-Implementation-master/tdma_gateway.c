#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <net/if.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <linux/if_tun.h>

#define TDMA_MAGIC 0x54444D41u /* "TDMA" */
#define TDMA_VERSION 1u
#define TDMA_TYPE_DATA 1u
#define MAX_PAYLOAD 1400u
#define MAX_QUEUE 256u
#define DEFAULT_FRAME_US 100000u
#define DEFAULT_CONTENTION_US 0000u
#define DEFAULT_GUARD_US 6000u
#define DEFAULT_GNU_PORT 17001u
#define GNU_ACCESS_CODE 0xE546BA38u

#pragma pack(push, 1)
struct gr_preamble {
    uint32_t access_code;
    uint16_t bit_len_1;
    uint16_t bit_len_2;
};

struct tdma_header_wire {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t header_len;
    uint32_t frame_id;
    uint16_t slot_id;
    uint16_t node_id;
    uint32_t seq;
    uint16_t payload_len;
    uint16_t flags;
    uint64_t tx_timestamp_us;
    uint32_t payload_crc32;
};
#pragma pack(pop)

struct queued_packet {
    uint8_t data[MAX_PAYLOAD];
    size_t len;
    uint64_t rx_timestamp_us;
};

struct packet_queue {
    struct queued_packet q[MAX_QUEUE];
    size_t head, tail, count;
};

struct config {
    unsigned node_id;
    unsigned nodes;
    uint64_t frame_us;
    uint64_t contention_us;
    uint64_t guard_us;
    uint16_t gnu_port;
    char gnu_ip[INET_ADDRSTRLEN];
    char tap_name[IFNAMSIZ];
    int broadcast;
};

static volatile sig_atomic_t running = 1;

static void on_signal(int sig) { (void)sig; running = 0; }

static uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint64_t htonll(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t)htonl((uint32_t)x) << 32) | htonl((uint32_t)(x >> 32));
#else
    return x;
#endif
}

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1u)));
    }
    return ~crc;
}

static int open_tap_interface(const char *dev) {
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("Opening /dev/net/tun");
        return -1;
    }

    struct ifreq ifr = {0};
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("ioctl(TUNSETIFF)");
        close(fd);
        return -1;
    }
    return fd;
}

static int queue_push(struct packet_queue *pq, const uint8_t *data, size_t len) {
    if (len > MAX_PAYLOAD || pq->count == MAX_QUEUE) return -1;
    struct queued_packet *p = &pq->q[pq->tail];
    memcpy(p->data, data, len);
    p->len = len;
    p->rx_timestamp_us = now_us();
    pq->tail = (pq->tail + 1) % MAX_QUEUE;
    pq->count++;
    return 0;
}

static struct queued_packet *queue_front(struct packet_queue *pq) {
    return pq->count ? &pq->q[pq->head] : NULL;
}

static void queue_pop(struct packet_queue *pq) {
    if (!pq->count) return;
    pq->head = (pq->head + 1) % MAX_QUEUE;
    pq->count--;
}

static int configure_destination(struct sockaddr_in *dst, const struct config *c) {
    memset(dst, 0, sizeof(*dst));
    dst->sin_family = AF_INET;
    dst->sin_port = htons(c->gnu_port);
    if (inet_pton(AF_INET, c->gnu_ip, &dst->sin_addr) != 1) return -1;
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  --node-id N        TDMA node ID (default 1)\n"
        "  --nodes N          number of TDMA nodes (default 3)\n"
        "  --tap DEV          TAP interface name (default sdr0)\n"
        "  --gnu-ip IP        GNU Radio destination (default 127.0.0.1)\n"
        "  --gnu-port PORT    GNU Radio UDP port (default 17001)\n"
        "  --frame-us US      frame duration (default 300000)\n"
        "  --contention-us US contention region (default 100000)\n"
        "  --guard-us US      guard time (default 6000)\n"
        "  --broadcast        send to UDP broadcast address\n", prog);
}

static int parse_args(int argc, char **argv, struct config *c) {
    *c = (struct config){
        .node_id = 1, .nodes = 3, .frame_us = DEFAULT_FRAME_US,
        .contention_us = DEFAULT_CONTENTION_US, .guard_us = DEFAULT_GUARD_US,
        .gnu_port = DEFAULT_GNU_PORT, .gnu_ip = "127.0.0.1",
        .tap_name = "sdr0", .broadcast = 0
    };
    static const struct option opts[] = {
        {"node-id", required_argument, 0, 'n'}, {"nodes", required_argument, 0, 'N'},
        {"tap", required_argument, 0, 't'}, {"gnu-ip", required_argument, 0, 'i'},
        {"gnu-port", required_argument, 0, 'p'}, {"frame-us", required_argument, 0, 'f'},
        {"contention-us", required_argument, 0, 'c'}, {"guard-us", required_argument, 0, 'g'},
        {"broadcast", no_argument, 0, 'b'}, {0,0,0,0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "n:N:t:i:p:f:c:g:b", opts, NULL)) != -1) {
        unsigned long v;
        switch (opt) {
        case 'n': v = strtoul(optarg, NULL, 10); c->node_id = v; break;
        case 'N': v = strtoul(optarg, NULL, 10); c->nodes = v; break;
        case 't': strncpy(c->tap_name, optarg, sizeof(c->tap_name)-1); break;
        case 'i': strncpy(c->gnu_ip, optarg, sizeof(c->gnu_ip)-1); break;
        case 'p': v = strtoul(optarg, NULL, 10); c->gnu_port = v; break;
        case 'f': c->frame_us = strtoull(optarg, NULL, 10); break;
        case 'c': c->contention_us = strtoull(optarg, NULL, 10); break;
        case 'g': c->guard_us = strtoull(optarg, NULL, 10); break;
        case 'b': c->broadcast = 1; break;
        default: usage(argv[0]); return -1;
        }
    }
    if (!c->nodes || c->node_id == 0 || c->node_id > c->nodes ||
        c->contention_us >= c->frame_us || c->nodes > 65535 ||
        c->frame_us <= c->contention_us + c->guard_us) {
        fprintf(stderr, "Invalid TDMA configuration\n"); return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    struct config c;
    if (parse_args(argc, argv, &c) < 0) return EXIT_FAILURE;
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal);

    int tap_fd = open_tap_interface(c.tap_name);
    if (tap_fd < 0) return EXIT_FAILURE;

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (out_fd < 0) { perror("socket output"); close(tap_fd); return EXIT_FAILURE; }
    if (c.broadcast) { int yes = 1; setsockopt(out_fd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)); }

    struct sockaddr_in dst;
    if (configure_destination(&dst, &c) < 0) {
        fprintf(stderr, "Invalid GNU Radio IP: %s\n", c.gnu_ip);
        close(tap_fd); close(out_fd); return EXIT_FAILURE;
    }
    if (c.broadcast) dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    struct packet_queue queue = {0};
    uint32_t seq = 0, frame_id = 0;
    uint64_t slot_us = (c.frame_us - c.contention_us) / c.nodes;
    uint64_t active_slot_us = slot_us > c.guard_us ? slot_us - c.guard_us : 0;
    fprintf(stderr, "TDMA gateway: node=%u/%u frame=%lluus slot=%lluus active=%lluus TAP=%s GNU-Radio=%s:%u\n",
            c.node_id, c.nodes, (unsigned long long)c.frame_us,
            (unsigned long long)slot_us, (unsigned long long)active_slot_us,
            c.tap_name, c.gnu_ip, c.gnu_port);

    uint64_t next_stats = now_us() + 1000000ULL;
    uint64_t rx_count = 0, tx_count = 0, drop_count = 0;

    while (running) {
        uint64_t now = now_us();
        uint64_t frame_pos = now % c.frame_us;
        uint64_t schedule_pos = frame_pos >= c.contention_us ? frame_pos - c.contention_us : UINT64_MAX;
        uint64_t my_slot_start = (uint64_t)(c.node_id - 1) * slot_us;
        int in_my_slot = schedule_pos != UINT64_MAX &&
                         schedule_pos >= my_slot_start &&
                         schedule_pos < my_slot_start + active_slot_us;
        uint32_t current_frame = (uint32_t)(now / c.frame_us);
        if (current_frame != frame_id) frame_id = current_frame;

        /* Drain TAP ingress interface into the queue */
        struct timeval tv = {0, 0};
        fd_set rfds; FD_ZERO(&rfds); FD_SET(tap_fd, &rfds);
        int r = select(tap_fd + 1, &rfds, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(tap_fd, &rfds)) {
            uint8_t buf[MAX_PAYLOAD];
            ssize_t n = read(tap_fd, buf, sizeof(buf));
            
            /* Minimum Ethernet frame size is 14 bytes (MAC dest + MAC src + EtherType) */
            if (n >= 14) {
                /* Filter Broadcast (FF:FF:FF...) and Multicast (01:00:5E...) */
                int is_multicast_or_broadcast = (buf[0] & 0x01);
                
                if (!is_multicast_or_broadcast) {
                    rx_count++;
                    if (queue_push(&queue, buf, (size_t)n) < 0) drop_count++;
                }
            }
        }

        /* Transmit queued packet inside our assigned TDMA slot */
        if (in_my_slot && queue.count) {
            struct queued_packet *qp = queue_front(&queue);
            uint16_t inner_payload_len = (uint16_t)(sizeof(struct tdma_header_wire) + qp->len);
            uint8_t wire[sizeof(struct gr_preamble) + sizeof(struct tdma_header_wire) + MAX_PAYLOAD];
            
            struct gr_preamble gr_hdr;
            gr_hdr.access_code = htonl(GNU_ACCESS_CODE);
            gr_hdr.bit_len_1   = htons(inner_payload_len);
            gr_hdr.bit_len_2   = htons(inner_payload_len);

            struct tdma_header_wire h = {0};
            h.magic = htonl(TDMA_MAGIC);
            h.version = TDMA_VERSION;
            h.type = TDMA_TYPE_DATA;
            h.header_len = htons(sizeof(h));
            h.frame_id = htonl(frame_id);
            h.slot_id = htons((uint16_t)(c.node_id - 1));
            h.node_id = htons((uint16_t)c.node_id);
            h.seq = htonl(seq++);
            h.payload_len = htons((uint16_t)qp->len);
            h.flags = htons(0);
            h.tx_timestamp_us = htonll(now_us());
            h.payload_crc32 = htonl(crc32(qp->data, qp->len));

            uint8_t *ptr = wire;
            memcpy(ptr, &gr_hdr, sizeof(gr_hdr));      ptr += sizeof(gr_hdr);
            memcpy(ptr, &h, sizeof(h));                ptr += sizeof(h);
            memcpy(ptr, qp->data, qp->len);            ptr += qp->len;

            size_t total_wire_bytes = ptr - wire;

            ssize_t sent = sendto(out_fd, wire, total_wire_bytes, 0, (struct sockaddr *)&dst, sizeof(dst));
            if (sent == (ssize_t)total_wire_bytes) { 
                tx_count++; 
                queue_pop(&queue); 
            }
            else if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("sendto GNU Radio");
            }
        } else {
            struct timespec ts = {0, 500000};
            nanosleep(&ts, NULL);
        }

        if (now_us() >= next_stats) {
            fprintf(stderr, "stats unicast_rx=%llu tx=%llu queued=%zu dropped=%llu frame=%u\n",
                    (unsigned long long)rx_count, (unsigned long long)tx_count,
                    queue.count, (unsigned long long)drop_count, frame_id);
            next_stats += 1000000ULL;
        }
    }

    close(tap_fd); close(out_fd);
    fprintf(stderr, "TDMA gateway stopped. unicast_rx=%llu tx=%llu dropped=%llu\n",
            (unsigned long long)rx_count, (unsigned long long)tx_count,
            (unsigned long long)drop_count);
    return 0;
}
