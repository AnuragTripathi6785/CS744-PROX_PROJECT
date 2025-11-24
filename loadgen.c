#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    char host[256];
    char port[8];
} url_info;

typedef struct {
    pthread_mutex_t lock;
    size_t requests;
    size_t errors;
    double *latencies;
    size_t lat_capacity;
    size_t lat_size;
    double lat_sum;
} stats_t;

typedef struct {
    url_info url;
    int hot_keys;
    int value_bytes;
    bool is_putall;
    volatile bool *stop_flag;
    stats_t *stats;
    long long *put_counter;
    pthread_mutex_t *counter_lock;
    unsigned int rng_seed;
} worker_args_t;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static bool parse_url(const char *url, url_info *out) {
    const char *prefix = "http://";
    size_t prelen = strlen(prefix);
    if (strncmp(url, prefix, prelen) != 0) return false;
    const char *hostport = url + prelen;
    const char *colon = strchr(hostport, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - hostport);
        if (hlen >= sizeof(out->host)) return false;
        memcpy(out->host, hostport, hlen);
        out->host[hlen] = '\0';
        const char *port = colon + 1;
        size_t plen = strlen(port);
        if (plen == 0 || plen >= sizeof(out->port)) return false;
        memcpy(out->port, port, plen);
        out->port[plen] = '\0';
    } else {
        if (strlen(hostport) >= sizeof(out->host)) return false;
        strcpy(out->host, hostport);
        strcpy(out->port, "80");
    }
    return true;
}

static int connect_tcp(const url_info *info) {
    struct addrinfo hints = {0}, *res = NULL, *rp = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int err = getaddrinfo(info->host, info->port, &hints, &res);
    if (err != 0) return -1;
    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

static int read_response(int fd) {
    char buf[8192];
    ssize_t nread = 0;
    int status = -1;
    size_t header_end = 0;
    ssize_t content_length = -1;
    while (1) {
        ssize_t n = recv(fd, buf + nread, sizeof(buf) - nread, 0);
        if (n <= 0) return -1;
        nread += n;
        char *hdr_end_ptr = NULL;
        hdr_end_ptr = strstr(buf, "\r\n\r\n");
        if (hdr_end_ptr) {
            header_end = (size_t)(hdr_end_ptr - buf) + 4;
            // parse status
            if (sscanf(buf, "HTTP/%*s %d", &status) != 1) status = -1;
            // parse content-length if present
            char *cl = strcasestr(buf, "Content-Length:");
            if (cl) {
                cl += strlen("Content-Length:");
                while (*cl == ' ') cl++;
                content_length = strtol(cl, NULL, 10);
            }
            break;
        }
        if ((size_t)nread == sizeof(buf)) return -1;  // header too large
    }
    ssize_t body_read = (ssize_t)(nread - (ssize_t)header_end);
    if (content_length > body_read) {
        ssize_t remaining = content_length - body_read;
        while (remaining > 0) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) return -1;
            remaining -= n;
        }
    }
    return status;
}

static void stats_record(stats_t *s, double latency, bool ok) {
    pthread_mutex_lock(&s->lock);
    s->requests++;
    if (!ok) {
        s->errors++;
    } else {
        if (s->lat_size == s->lat_capacity) {
            size_t newcap = s->lat_capacity ? s->lat_capacity * 2 : 1024;
            double *tmp = realloc(s->latencies, newcap * sizeof(double));
            if (!tmp) {
                pthread_mutex_unlock(&s->lock);
                return;
            }
            s->latencies = tmp;
            s->lat_capacity = newcap;
        }
        s->latencies[s->lat_size++] = latency;
        s->lat_sum += latency;
    }
    pthread_mutex_unlock(&s->lock);
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static double percentile(double *vals, size_t n, double pct) {
    if (n == 0) return 0.0;
    double pos = pct / 100.0 * (n - 1);
    size_t idx = (size_t)pos;
    size_t idx2 = idx + 1 < n ? idx + 1 : idx;
    double frac = pos - idx;
    return vals[idx] * (1.0 - frac) + vals[idx2] * frac;
}

static void *worker_thread(void *arg) {
    worker_args_t *w = (worker_args_t *)arg;
    char host_header[300];
    snprintf(host_header, sizeof(host_header), "Host: %s\r\n", w->url.host);
    char *payload = NULL;
    if (w->is_putall) {
        payload = malloc((size_t)w->value_bytes);
        if (!payload) return NULL;
        memset(payload, 'x', (size_t)w->value_bytes);
    }
    while (!*(w->stop_flag)) {
        int fd = connect_tcp(&w->url);
        if (fd < 0) break;
        double start = now_sec();
        bool ok = true;
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        struct linger ling = {1, 0};
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
        if (w->is_putall) {
            long long id;
            pthread_mutex_lock(w->counter_lock);
            id = (*w->put_counter)++;
            pthread_mutex_unlock(w->counter_lock);
            char path[128];
            snprintf(path, sizeof(path), "/putall_%lld", id);
            char req[512];
            int len = snprintf(req, sizeof(req),
                               "PUT %s HTTP/1.1\r\n%s"
                               "Content-Length: %d\r\n"
                               "Connection: close\r\n\r\n",
                               path, host_header, w->value_bytes);
            if (send(fd, req, len, 0) != len || send(fd, payload, w->value_bytes, 0) != w->value_bytes) {
                ok = false;
            } else {
                int status = read_response(fd);
                if (status < 0 || status >= 500) ok = false;
            }
        } else {
            int key = (rand_r(&w->rng_seed) % w->hot_keys) + 1;
            char path[64];
            snprintf(path, sizeof(path), "/hot%d", key);
            char req[256];
            int len = snprintf(req, sizeof(req),
                               "GET %s HTTP/1.1\r\n%s"
                               "Connection: close\r\n\r\n",
                               path, host_header);
            if (send(fd, req, len, 0) != len) {
                ok = false;
            } else {
                int status = read_response(fd);
                if (status < 0 || status >= 500) ok = false;
            }
        }
        close(fd);
        double end = now_sec();
        stats_record(w->stats, end - start, ok);
    }
    if (payload) free(payload);
    return NULL;
}

static void preload_hot(const url_info *info, int hot_keys) {
    char host_header[300];
    snprintf(host_header, sizeof(host_header), "Host: %s\r\n", info->host);
    const char *body = "warm";
    for (int i = 1; i <= hot_keys; i++) {
        int fd = connect_tcp(info);
        if (fd < 0) return;
        char path[64];
        snprintf(path, sizeof(path), "/hot%d", i);
        char req[512];
        int len = snprintf(req, sizeof(req),
                           "PUT %s HTTP/1.1\r\n%s"
                           "Content-Length: %zu\r\n"
                           "Connection: close\r\n\r\n",
                           path, host_header, strlen(body));
        send(fd, req, len, 0);
        send(fd, body, strlen(body), 0);
        read_response(fd);
        close(fd);
    }
}

static void print_report(const char *name, int threads, double duration, stats_t *s) {
    pthread_mutex_lock(&s->lock);
    size_t total = s->requests;
    size_t errors = s->errors;
    size_t n = s->lat_size;
    double lat_sum = s->lat_sum;
    double *vals = NULL;
    if (n > 0) {
        vals = malloc(n * sizeof(double));
        if (vals) memcpy(vals, s->latencies, n * sizeof(double));
    }
    pthread_mutex_unlock(&s->lock);

    if (vals) qsort(vals, n, sizeof(double), cmp_double);
    double success = (double)(total - errors);
    double rps = duration > 0 ? success / duration : 0.0;
    double avg = n > 0 ? lat_sum / n : 0.0;
    double p50 = vals ? percentile(vals, n, 50.0) : 0.0;
    double p95 = vals ? percentile(vals, n, 95.0) : 0.0;
    double p99 = vals ? percentile(vals, n, 99.0) : 0.0;

    printf("=== %s results ===\n", name);
    printf("threads=%d duration=%.2fs\n", threads, duration);
    printf("requests=%zu success=%zu errors=%zu\n", total, total - errors, errors);
    printf("throughput=%.1f req/s\n", rps);
    printf("latency_avg=%.2f ms p50=%.2f ms p95=%.2f ms p99=%.2f ms\n",
           avg * 1000.0, p50 * 1000.0, p95 * 1000.0, p99 * 1000.0);

    free(vals);
}

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage:\n"
            "  %s getpopular --url http://host:port --threads N --duration SECONDS --hot-keys N\n"
            "  %s putall     --url http://host:port --threads N --duration SECONDS --value-bytes BYTES\n",
            prog, prog);
}

int main(int argc, char **argv) {
    /* Avoid termination on closed sockets */
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    const char *mode = argv[1];
    url_info info = {.host = "localhost", .port = "8080"};
    int threads = 4;
    double duration = 30.0;
    int hot_keys = 10;
    int value_bytes = 512;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
            if (!parse_url(argv[++i], &info)) {
                fprintf(stderr, "Invalid URL (http://host:port)\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            threads = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atof(argv[++i]);
        } else if (strcmp(argv[i], "--hot-keys") == 0 && i + 1 < argc) {
            hot_keys = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--value-bytes") == 0 && i + 1 < argc) {
            value_bytes = atoi(argv[++i]);
        }
    }

    bool is_putall = strcmp(mode, "putall") == 0;
    bool is_getpopular = strcmp(mode, "getpopular") == 0;
    if (!is_putall && !is_getpopular) {
        usage(argv[0]);
        return 1;
    }

    if (is_getpopular) preload_hot(&info, hot_keys);

    stats_t stats = {0};
    pthread_mutex_init(&stats.lock, NULL);

    volatile bool stop_flag = false;
    long long counter = 0;
    pthread_mutex_t counter_lock;
    pthread_mutex_init(&counter_lock, NULL);

    pthread_t *tids = calloc((size_t)threads, sizeof(pthread_t));
    worker_args_t *args = calloc((size_t)threads, sizeof(worker_args_t));
    if (!tids || !args) {
        fprintf(stderr, "Allocation failure\n");
        return 1;
    }

    for (int i = 0; i < threads; i++) {
        args[i].url = info;
        args[i].hot_keys = hot_keys;
        args[i].value_bytes = value_bytes;
        args[i].is_putall = is_putall;
        args[i].stop_flag = &stop_flag;
        args[i].stats = &stats;
        args[i].put_counter = &counter;
        args[i].counter_lock = &counter_lock;
        args[i].rng_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self() ^ (unsigned int)i;
        pthread_create(&tids[i], NULL, worker_thread, &args[i]);
    }

    double start = now_sec();
    while (now_sec() - start < duration) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&ts, NULL);
    }
    stop_flag = true;
    for (int i = 0; i < threads; i++) pthread_join(tids[i], NULL);

    print_report(mode, threads, duration, &stats);

    free(stats.latencies);
    free(tids);
    free(args);
    pthread_mutex_destroy(&stats.lock);
    pthread_mutex_destroy(&counter_lock);
    return 0;
}
