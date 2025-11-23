#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <libpq-fe.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 8192
#define CACHE_CAPACITY 200
#define HASH_SIZE 1031
#define MAX_THREADS 8
#define MAX_QUEUE 256
#define DB_CONNINFO "host=localhost port=5432 dbname=proxydb user=proxyuser password=proxypass"

typedef struct Cacheline
{
    char *key;
    char *value;
    size_t value_len;
    struct Cacheline *prev, *next, *hnext;
} Cacheline;

/* ---------- globals ---------- */
static Cacheline *lru_head = NULL, *lru_tail = NULL;
static Cacheline *hash_table[HASH_SIZE] = {0};
static int current_cache_count = 0;
static pthread_rwlock_t cache_rwlock = PTHREAD_RWLOCK_INITIALIZER;
/* statistics */
static atomic_ulong cache_hit = 0, cache_miss = 0;
static atomic_ulong db_reads = 0, db_writes = 0;
static atomic_ulong total_gets = 0, total_puts = 0, total_deletes = 0;
static const char *db_conninfo_str = DB_CONNINFO;

/* DB pool */
// perthread db connection
static __thread PGconn *thread_conn = NULL;
static PGconn *db_conn_pool[10];
static int db_pool_size = 10;
static pthread_mutex_t db_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static int pool_index = 0;

/* task queue */
typedef struct
{
    int fds[MAX_QUEUE];
    int front, rear, count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty, not_full;
} task_queue_t;

static task_queue_t queue = {
    .front = 0, .rear = 0, .count = 0, .lock = PTHREAD_MUTEX_INITIALIZER, .not_empty = PTHREAD_COND_INITIALIZER, .not_full = PTHREAD_COND_INITIALIZER};

/* shutdown support */
static volatile sig_atomic_t stop_server = 0;
static int shutdown_pipe[2] = {-1, -1}; // pipe to wake select()

/* signal handler - sets flag and writes to pipe to wake select() */
static void handle_signal(int sig)
{
    (void)sig;
    stop_server = 1;
    // best-effort write to wake select()
    if (shutdown_pipe[1] != -1)
    {
        ssize_t r = write(shutdown_pipe[1], "x", 1);
        (void)r;
    }
}

/* safe write loop */
static ssize_t write_complete(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}

/*  DB pool/init/ops  */

static int init_db_pool(void)
{
    for (int i = 0; i < db_pool_size; ++i)
    {
        db_conn_pool[i] = PQconnectdb(db_conninfo_str);
        if (PQstatus(db_conn_pool[i]) != CONNECTION_OK)
        {
            fprintf(stderr, "DB connection %d failed: %s\n", i, PQerrorMessage(db_conn_pool[i]));
            return -1;
        }
    }
    /* create table if not exists */
    const char *create_table =
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key VARCHAR(1024) PRIMARY KEY,"
        "value BYTEA,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
    PGresult *res = PQexec(db_conn_pool[0], create_table);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "CREATE TABLE failed: %s\n", PQerrorMessage(db_conn_pool[0]));
        PQclear(res);
        return -1;
    }
    PQclear(res);
    return 0;
}

static PGconn *get_db_connection(void)
{
    pthread_mutex_lock(&db_pool_lock);
    PGconn *c = db_conn_pool[pool_index];
    pool_index = (pool_index + 1) % db_pool_size;
    pthread_mutex_unlock(&db_pool_lock);
    return c;
}

/* write (binary) value using decode('..','escape') method after PQescapeStringConn */
static int db_write_kv(const char *key, const char *value, size_t value_len)
{
    PGconn *conn = thread_conn;
    if (!conn || PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "DB write: invalid connection\n");
        return -1;
    }
    const char *paramValues[2] = {key, value};
    int paramLengths[2] = {(int)strlen(key), (int)value_len};
    int paramFormats[2] = {0, 1}; /* key text, value binary */
    PGresult *res = PQexecParams(
        conn,
        "INSERT INTO kv_store (key, value, updated_at) "
        "VALUES ($1, $2::bytea, CURRENT_TIMESTAMP) "
        "ON CONFLICT (key) DO UPDATE SET value = $2::bytea, updated_at = CURRENT_TIMESTAMP;",
        2,
        NULL,
        paramValues,
        paramLengths,
        paramFormats,
        0);
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        fprintf(stderr, "DB write failed for key %s: %s\n", key, PQerrorMessage(conn));
        PQclear(res);
        return -1;
    }
    PQclear(res);
    atomic_fetch_add(&db_writes, 1);
    return 0;
}

static char *db_read_kv(const char *key, size_t *out_len)
{
    PGconn *conn = thread_conn;
    if (!conn || PQstatus(conn) != CONNECTION_OK)
        return NULL;
    const char *params[1] = {key};
    PGresult *res = PQexecParams(conn, "SELECT value FROM kv_store WHERE key=$1;", 1, NULL, params, NULL, NULL, 1);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        PQclear(res);
        return NULL;
    }
    if (PQntuples(res) == 0)
    {
        PQclear(res);
        return NULL;
    }
    int len = PQgetlength(res, 0, 0);
    char *val = malloc(len);
    if (!val)
    {
        PQclear(res);
        return NULL;
    }
    memcpy(val, PQgetvalue(res, 0, 0), len);
    *out_len = (size_t)len;
    PQclear(res);
    atomic_fetch_add(&db_reads, 1);
    return val;
}

static int db_delete_kv(const char *key)
{
    PGconn *conn = thread_conn;
    if (!conn)
        return -1;
    const char *params[1] = {key};
    PGresult *res = PQexecParams(conn, "DELETE FROM kv_store WHERE key=$1;", 1, NULL, params, NULL, NULL, 0);
    PQclear(res);
    return 0;
}

/*  cache helpers  */

static unsigned long hash_key(const char *s)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++))
        h = ((h << 5) + h) + c;
    return h % HASH_SIZE;
}

static void remove_from_list(Cacheline *node)
{
    if (!node)
        return;
    if (node->prev)
        node->prev->next = node->next;
    else
        lru_head = node->next;
    if (node->next)
        node->next->prev = node->prev;
    else
        lru_tail = node->prev;
    node->prev = node->next = NULL;
}

static void insert_into_list_front(Cacheline *node)
{
    node->prev = NULL;
    node->next = lru_head;
    if (lru_head)
        lru_head->prev = node;
    lru_head = node;
    if (!lru_tail)
        lru_tail = node;
}

static Cacheline *cache_lookup(const char *key)
{
    unsigned long h = hash_key(key);
    Cacheline *cur = hash_table[h];
    while (cur)
    {
        if (strcmp(cur->key, key) == 0)
            return cur;
        cur = cur->hnext;
    }
    return NULL;
}

static char *cache_read(const char *key, size_t *out_len)
{
    pthread_rwlock_rdlock(&cache_rwlock);
    Cacheline *node = cache_lookup(key);
    if (!node)
    {
        pthread_rwlock_unlock(&cache_rwlock);
        return NULL;
    }
    remove_from_list(node);
    insert_into_list_front(node);
    *out_len = node->value_len;
    char *v = node->value;
    pthread_rwlock_unlock(&cache_rwlock);
    return v;
}

static void evict_if_needed()
{
    while (current_cache_count > CACHE_CAPACITY)
    {
        Cacheline *n = lru_tail;
        if (!n)
            return;
        unsigned long h = hash_key(n->key);
        Cacheline *cur = hash_table[h], *prev = NULL;
        while (cur)
        {
            if (cur == n)
            {
                if (prev)
                    prev->hnext = cur->hnext;
                else
                    hash_table[h] = cur->hnext;
                break;
            }
            prev = cur;
            cur = cur->hnext;
        }
        remove_from_list(n);
        free(n->key);
        free(n->value);
        free(n);
        current_cache_count--;
    }
}

static int cache_write(const char *key, const char *val, size_t len)
{
    pthread_rwlock_wrlock(&cache_rwlock);
    Cacheline *e = cache_lookup(key);
    if (e)
    {
        free(e->value);
        e->value = malloc(len);
        if (!e->value)
        {
            pthread_rwlock_unlock(&cache_rwlock);
            return -1;
        }
        memcpy(e->value, val, len);
        e->value_len = len;
        remove_from_list(e);
        insert_into_list_front(e);
        pthread_rwlock_unlock(&cache_rwlock);
        return 0;
    }
    Cacheline *n = malloc(sizeof(Cacheline));
    if (!n)
    {
        pthread_rwlock_unlock(&cache_rwlock);
        return -1;
    }
    n->key = strdup(key);
    n->value = malloc(len);
    if (!n->key || !n->value)
    {
        free(n->key);
        free(n->value);
        free(n);
        pthread_rwlock_unlock(&cache_rwlock);
        return -1;
    }
    memcpy(n->value, val, len);
    n->value_len = len;
    n->prev = n->next = NULL;
    unsigned long h = hash_key(key);
    n->hnext = hash_table[h];
    hash_table[h] = n;
    insert_into_list_front(n);
    current_cache_count++;
    evict_if_needed();
    pthread_rwlock_unlock(&cache_rwlock);
    return 0;
}

static int cache_delete(const char *key)
{
    pthread_rwlock_wrlock(&cache_rwlock);
    Cacheline *node = cache_lookup(key);
    if (!node)
    {
        pthread_rwlock_unlock(&cache_rwlock);
        return 0;
    }
    unsigned long h = hash_key(key);
    Cacheline *cur = hash_table[h], *prev = NULL;
    while (cur)
    {
        if (cur == node)
        {
            if (prev)
                prev->hnext = cur->hnext;
            else
                hash_table[h] = cur->hnext;
            break;
        }
        prev = cur;
        cur = cur->hnext;
    }
    remove_from_list(node);
    free(node->key);
    free(node->value);
    free(node);
    current_cache_count--;
    pthread_rwlock_unlock(&cache_rwlock);
    return 0;
}

/*  task queue ops  */
static void enqueue_task(int fd)
{
    pthread_mutex_lock(&queue.lock);
    while (queue.count == MAX_QUEUE && !stop_server)
        pthread_cond_wait(&queue.not_full, &queue.lock);
    if (stop_server)
    {
        pthread_mutex_unlock(&queue.lock);
        close(fd);
        return;
    }
    queue.fds[queue.rear] = fd;
    queue.rear = (queue.rear + 1) % MAX_QUEUE;
    queue.count++;
    pthread_cond_signal(&queue.not_empty);
    pthread_mutex_unlock(&queue.lock);
}

static int dequeue_task(void)
{
    pthread_mutex_lock(&queue.lock);
    while (queue.count == 0 && !stop_server)
        pthread_cond_wait(&queue.not_empty, &queue.lock);
    if (stop_server && queue.count == 0)
    {
        pthread_mutex_unlock(&queue.lock);
        return -1;
    }
    int fd = queue.fds[queue.front];
    queue.front = (queue.front + 1) % MAX_QUEUE;
    queue.count--;
    pthread_cond_signal(&queue.not_full);
    pthread_mutex_unlock(&queue.lock);
    return fd;
}

/*  client handler  */
/* Robust client handler: reads headers fully, parses Content-Length, reads body reliably. */
static void handle_client_fd(int client_fd)
{
    if (!thread_conn)
    {
        thread_conn = PQconnectdb(db_conninfo_str);
        if (PQstatus(thread_conn) != CONNECTION_OK)
        {
            fprintf(stderr, "Thread DB connection failed: %s\n", PQerrorMessage(thread_conn));
            close(client_fd);
            return;
        }
    }

    /* Read headers (loop until "\r\n\r\n") into a dynamically grown buffer */
    size_t hdr_cap = 8192;
    size_t hdr_len = 0;
    char *hdr = malloc(hdr_cap);
    if (!hdr)
    {
        close(client_fd);
        return;
    }

    ssize_t n;
    int header_complete = 0;
    while (!header_complete)
    {
        if (hdr_len + 4096 > hdr_cap)
        {
            size_t newcap = hdr_cap * 2;
            char *tmp = realloc(hdr, newcap);
            if (!tmp)
            {
                free(hdr);
                close(client_fd);
                return;
            }
            hdr = tmp;
            hdr_cap = newcap;
        }
        n = read(client_fd, hdr + hdr_len, 4096);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            free(hdr);
            close(client_fd);
            return;
        }
        if (n == 0)
        {
            /* client closed connection before sending anything */
            free(hdr);
            close(client_fd);
            return;
        }
        hdr_len += (size_t)n;
        hdr[hdr_len] = '\0';
        if (hdr_len >= 4)
        {
            if (strstr(hdr, "\r\n\r\n") != NULL)
                header_complete = 1;
        }
        /* safety: cap headers at 64KB */
        if (hdr_len > 65536)
        {
            free(hdr);
            close(client_fd);
            return;
        }
    }

    /* parse method and path */
    char method[16] = {0}, path[1024] = {0};
    if (sscanf(hdr, "%15s %1023s", method, path) != 2)
    {
        free(hdr);
        close(client_fd);
        return;
    }

    /* Stats endpoint first (no body needed) */
    if (strcmp(path, "/__stats") == 0)
    {
        unsigned long h = atomic_load(&cache_hit), m = atomic_load(&cache_miss);
        unsigned long total = h + m;
        double ratio = (total > 0) ? ((double)h / total * 100.0) : 0.0;
        char resp[512];
        int len = snprintf(resp, sizeof(resp),
                           "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n"
                           "Cache Hits: %lu\nCache Misses: %lu\nCache Hit Ratio: %.2f%%\nDB Reads: %lu\nDB Writes: %lu\n",
                           (size_t)((size_t)snprintf(NULL, 0,
                                                     "Cache Hits: %lu\nCache Misses: %lu\nCache Hit Ratio: %.2f%%\nDB Reads: %lu\nDB Writes: %lu\n",
                                                     h, m, ratio, atomic_load(&db_reads), atomic_load(&db_writes))),
                           h, m, ratio, atomic_load(&db_reads), atomic_load(&db_writes));
        write_complete(client_fd, resp, (size_t)len);
        free(hdr);
        close(client_fd);
        return;
    }

    /* Find header end */
    char *hdr_end = strstr(hdr, "\r\n\r\n");
    size_t header_bytes = (hdr_end ? (size_t)(hdr_end + 4 - hdr) : hdr_len);
    /* Parse Content-Length (case-insensitive) */
    int content_length = 0;
    char *p = hdr;
    while (p && (p < hdr + header_bytes))
    {
        /* find end of this header line */
        char *line_end = strstr(p, "\r\n");
        if (!line_end)
            break;
        size_t linelen = (size_t)(line_end - p);
        if (linelen > 15) /* quick filter */
        {
            if (strncasecmp(p, "Content-Length:", 15) == 0)
            {
                /* skip past colon */
                char *val = p + 15;
                while (*val == ' ' || *val == '\t')
                    val++;
                content_length = atoi(val);
                break;
            }
        }
        p = line_end + 2;
    }

    /* Handle methods */
    if (strcasecmp(method, "PUT") == 0)
    {
        atomic_fetch_add(&total_puts, 1);

        /* Require Content-Length for PUT */
        if (content_length <= 0)
        {
            const char *resp = "HTTP/1.1 411 Length Required\r\nContent-Length: 0\r\n\r\n";
            write_complete(client_fd, resp, strlen(resp));
            free(hdr);
            close(client_fd);
            return;
        }

        /* Allocate buffer for body and copy any body bytes already read after header */
        char *body_buf = malloc((size_t)content_length + 1);
        if (!body_buf)
        {
            free(hdr);
            close(client_fd);
            return;
        }

        size_t already = 0;
        if (hdr_end)
        {
            /* bytes after header_end in hdr buffer are part of the body */
            size_t tail_bytes = hdr_len - header_bytes;
            if (tail_bytes > 0)
            {
                size_t tocopy = tail_bytes;
                if (tocopy > (size_t)content_length)
                    tocopy = (size_t)content_length;
                memcpy(body_buf, hdr + header_bytes, tocopy);
                already = tocopy;
            }
        }

        /* read remaining bytes */
        size_t total = already;
        while (total < (size_t)content_length)
        {
            ssize_t r = read(client_fd, body_buf + total, (size_t)content_length - total);
            if (r < 0)
            {
                if (errno == EINTR)
                    continue;
                break;
            }
            if (r == 0)
                break; /* client closed early */
            total += (size_t)r;
        }

        /* if we didn't get full body, fail gracefully */
        if (total < (size_t)content_length)
        {
            const char *resp = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
            write_complete(client_fd, resp, strlen(resp));
            free(body_buf);
            free(hdr);
            close(client_fd);
            return;
        }

        /* Null-terminate for safety in case code expects strings (values may be binary) */
        body_buf[content_length] = '\0';

        /* Write to DB and update cache */
        if (db_write_kv(path, body_buf, (size_t)content_length) == 0)
        {
            cache_write(path, body_buf, (size_t)content_length);
            const char *ok = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\nOK";
            write_complete(client_fd, ok, strlen(ok));
        }
        else
        {
            const char *err = "HTTP/1.1 500 Internal Server Error\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
            write_complete(client_fd, err, strlen(err));
        }

        free(body_buf);
        free(hdr);
        close(client_fd);
        return;
    }
    else if (strcasecmp(method, "GET") == 0)
    {
        atomic_fetch_add(&total_gets, 1);

        size_t len = 0;
        char *val = cache_read(path, &len);
        if (val)
        {
            atomic_fetch_add(&cache_hit, 1);
            char header[160];
            int h = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n", len);
            write_complete(client_fd, header, (size_t)h);
            write_complete(client_fd, val, len);
            free(hdr);
            close(client_fd);
            return;
        }

        atomic_fetch_add(&cache_miss, 1);
        size_t db_len = 0;
        char *dbv = db_read_kv(path, &db_len);
        if (dbv)
        {
            cache_write(path, dbv, db_len);
            char header[160];
            int h = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: %zu\r\n\r\n", db_len);
            write_complete(client_fd, header, (size_t)h);
            write_complete(client_fd, dbv, db_len);
            free(dbv);
            free(hdr);
            close(client_fd);
            return;
        }
        else
        {
            const char *nf = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 9\r\n\r\nNot Found";
            write_complete(client_fd, nf, strlen(nf));
            free(hdr);
            close(client_fd);
            return;
        }
    }
    else if (strcasecmp(method, "DELETE") == 0)
    {
        atomic_fetch_add(&total_deletes, 1);
        db_delete_kv(path);
        cache_delete(path);
        const char *ok = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 7\r\n\r\nDELETED";
        write_complete(client_fd, ok, strlen(ok));
        free(hdr);
        close(client_fd);
        return;
    }

    /* Unknown method */
    free(hdr);
    close(client_fd);
    return;
}

/* worker thread */
static void *worker_main(void *arg)
{

    (void)arg;
    while (!stop_server)
    {
        int fd = dequeue_task();
        if (fd < 0)
            break;
        handle_client_fd(fd);
    }
    return NULL;
}

/* ---------- main ---------- */
int main(void)
{

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if (pipe(shutdown_pipe) == -1)
    {
        perror("pipe");
        return 1;
    }

    if (init_db_pool() != 0)
    {
        fprintf(stderr, "DB pool init failed. Make sure postgres is running and DB_CONNINFO is correct.\n");
        return 1;
    }

    /* create listening socket */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* set reuse options */
    int opt = 1;
#ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* allow DB_CONNINFO override via environment (e.g., DB_CONNINFO="host=localhost dbname=proxydb user=postgres password=postgres") */
    const char *env_conn = getenv("DB_CONNINFO");
    if (env_conn && *env_conn)
        db_conninfo_str = env_conn;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        if (errno == EADDRINUSE)
        {
            fprintf(stderr, "bind: Address already in use. Another process may be listening on port %d.\n", SERVER_PORT);
            fprintf(stderr, "Use: lsof -i :%d  (or) sudo lsof -i :%d and kill the PID, then retry.\n", SERVER_PORT, SERVER_PORT);
        }
        else
        {
            perror("bind");
        }
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 128) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    /* spawn worker threads */
    pthread_t workers[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i)
        pthread_create(&workers[i], NULL, worker_main, NULL);

    printf("Listening on port %d\nAccess stats at: http://localhost:%d/__stats\n", SERVER_PORT, SERVER_PORT);

    /* main loop uses select so it can be interrupted by shutdown_pipe */
    fd_set rfds;
    int maxfd = (server_fd > shutdown_pipe[0]) ? server_fd : shutdown_pipe[0];

    while (!stop_server)
    {
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        FD_SET(shutdown_pipe[0], &rfds);
        int sel = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (sel < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }
        if (FD_ISSET(shutdown_pipe[0], &rfds))
            break;
        if (FD_ISSET(server_fd, &rfds))
        {
            struct sockaddr_in cli;
            socklen_t cli_len = sizeof(cli);
            int cfd = accept(server_fd, (struct sockaddr *)&cli, &cli_len);
            if (cfd < 0)
            {
                if (errno == EINTR)
                    continue;
                continue;
            }
            enqueue_task(cfd);
        }
    }

    /* shutdown: wake workers, join, cleanup */
    pthread_mutex_lock(&queue.lock);
    stop_server = 1;
    pthread_cond_broadcast(&queue.not_empty);
    pthread_cond_broadcast(&queue.not_full);
    pthread_mutex_unlock(&queue.lock);

    for (int i = 0; i < MAX_THREADS; ++i)
        pthread_join(workers[i], NULL);

    for (int i = 0; i < db_pool_size; ++i)
        if (db_conn_pool[i])
            PQfinish(db_conn_pool[i]);

    close(server_fd);
    close(shutdown_pipe[0]);
    close(shutdown_pipe[1]);

    printf("[INFO] Server terminated gracefully.\n");
    return 0;
}
