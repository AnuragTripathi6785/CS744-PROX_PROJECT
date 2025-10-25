#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h> //for making the cache
#include <errno.h>

// defining the port numbers and other macros
#define PROXY_PORT 8080
#define ORIGIN_PORT 8000
#define BUFFER_SIZE 8192
#define CACHE_CAPACITY 5
#define HASH_SIZE 1031 // it's a prime number becaue we want to do uniform hashing

// structure of the cache:
typedef struct Cacheline
{
    char *path;              // requested path or url
    char *data;              // http response
    size_t data_len;         // length of the response
    struct Cacheline *prev;  // the previous line in the LRU list
    struct Cacheline *next;  // the next line in the LRU list
    struct Cacheline *hnext; // the next node in the hash chain
} Cacheline;

// defining all the global variables for chache
static Cacheline *lru_head = NULL;
static Cacheline *lru_tail = NULL;
static int current_cache_count;
static Cacheline *hash_table[HASH_SIZE] = {0};
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

// making the DJB2 hash algo to convert path/url into a an integer to be hashed into bucket
static unsigned long hash_path(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_SIZE;
}

// creating a doubly linked list for the LRU list
static void remove_from_list(Cacheline *node) // removing from the LRU list
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

static void insert_into_list(Cacheline *node) // inserting into the LRU list on the front
{
    node->prev = NULL;
    node->next = lru_head;
    if (lru_head)
        lru_head->prev = node;
    lru_head = node;
    if (!lru_tail)
        lru_tail = node;
}

// Eviction from the cache when necessary
static void evict()
{
    while (current_cache_count > CACHE_CAPACITY)
    {
        Cacheline *node = lru_tail;
        if (!node)
            return;
        unsigned long h = hash_path(node->path);
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
        free(node->path);
        free(node->data);
        free(node);
        current_cache_count--;
    }
}

// cache_look_up , read, write functions :
static Cacheline *cache_look_up(const char *path)
{
    unsigned long h = hash_path(path);
    Cacheline *cur = hash_table[h];
    while (cur)
    {
        if (strcmp(cur->path, path) == 0)
            return cur;
        cur = cur->hnext;
    }
    return NULL;
}

static char *cache_read(const char *path, size_t *data_len)
{
    pthread_mutex_lock(&cache_lock);
    Cacheline *node = cache_look_up(path);
    if (!node)
    {
        pthread_mutex_unlock(&cache_lock);
        return NULL;
    }
    remove_from_list(node);
    insert_into_list(node);
    *data_len = node->data_len;
    char *data = node->data;
    pthread_mutex_unlock(&cache_lock);
    return data;
}

static int cache_write(const char *path, const char *data, size_t data_len)
{
    pthread_mutex_lock(&cache_lock);
    Cacheline *existing = cache_look_up(path);
    if (existing)
    { /*replace the existing data*/
        free(existing->data);
        existing->data = malloc(data_len);
        if (!existing->data) // memory full
        {
            pthread_mutex_unlock(&cache_lock);
            return -1;
        }
        memcpy(existing->data, data, data_len);
        existing->data_len = data_len;
        remove_from_list(existing);
        insert_into_list(existing);
    }
    else
    { /*create a new node*/
        Cacheline *new_node = malloc(sizeof(Cacheline));
        if (!new_node)
        {
            pthread_mutex_unlock(&cache_lock);
            return -1;
        }
        new_node->data = malloc(data_len);
        new_node->path = strdup(path);
        if (!new_node->data || !new_node->path)
        {
            free(new_node->data);
            free(new_node->path);
            free(new_node);
            pthread_mutex_unlock(&cache_lock);
            return -1;
        }
        memcpy(new_node->data, data, data_len);
        new_node->data_len = data_len;
        new_node->prev = new_node->next = new_node->hnext = NULL;
        // inserting into the hash
        unsigned long h = hash_path(path);
        new_node->hnext = hash_table[h];
        hash_table[h] = new_node;
        insert_into_list(new_node);
        current_cache_count++;
        evict();
    }
    pthread_mutex_unlock(&cache_lock);
    return 0;
}

// writing all the data if it exceeds the buffer size: Handling partial responses
static size_t write_complete(int fd, const char *buffer, size_t length)
{
    size_t total = 0;
    while (total < length)
    {
        size_t n = write(fd, buffer + total, length - total);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        total += (size_t)n;
    }
    return total;
}

// client handler!!
void *client_handler(void *arg)
{
    int client_fd = *((int *)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    // reading the message from the client!
    ssize_t r = read(client_fd, buffer, sizeof(buffer) - 1);
    if (r <= 0)
    {
        close(client_fd);
        return NULL;
    }
    buffer[r] = '\0';
    printf("{Thread: %lu} handling the request:\n", pthread_self());

    // checking if it's a get request or any other
    char method[16], path[1024];
    sscanf(buffer, "%15s %1023s", method, path);
    printf("Method=%s Path=%s\n", method, path);
    if (strcmp(method, "GET") != 0)
    {
        const char *error_msg = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 22\r\n\r\nOnly GET supported for now!";
        write_complete(client_fd, error_msg, strlen(error_msg));
        close(client_fd);
        pthread_exit(NULL);
    }

    // trying to retrieve the data from the cache before contacting the origin server
    size_t cached_len;
    char *cached_data = cache_read(path, &cached_len);
    if (cached_data)
    {
        printf("Cache HIT!\n");
        write_complete(client_fd, cached_data, cached_len);
        close(client_fd);
        pthread_exit(NULL);
    }

    // on miss
    printf("Cache MISS for %s, fetching the data from origin server...\n", path);

    // establishing connection with the origin server on the cache miss
    int origin_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in origin_addr;
    memset(&origin_addr, 0, sizeof(origin_addr));
    origin_addr.sin_family = AF_INET;
    origin_addr.sin_port = htons(ORIGIN_PORT);
    inet_pton(AF_INET, "127.0.0.1", &origin_addr.sin_addr);

    if (connect(origin_fd, (struct sockaddr *)&origin_addr, sizeof(origin_addr)) < 0)
    {
        perror("connect to origin");
        close(origin_fd);
        close(client_fd);
        pthread_exit(NULL);
    }

    // sending the request from client to origin
    write_complete(origin_fd, buffer, r);

    // reading and fwding reply from origin to the client
    ssize_t n;

    char *accumulate = NULL;
    size_t accumulate_len = 0; // storing the data from origin so that we can cache it!!
    while ((n = read(origin_fd, buffer, sizeof(buffer))) > 0)
    {
        write_complete(client_fd, buffer, n);
        accumulate = realloc(accumulate, accumulate_len + n);
        memcpy(accumulate + accumulate_len, buffer, n);
        accumulate_len += n;
    }
    // storing the cache missed data into the cache
    if (accumulate && accumulate_len > 0)
    {
        cache_write(path, accumulate, accumulate_len);
    }
    free(accumulate);

    close(origin_fd);
    close(client_fd);
    pthread_exit(NULL);
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    server_fd = socket(AF_INET, SOCK_STREAM, 0); // tcp socket creation
    if (server_fd < 0)
    {
        perror("socket not found");
        exit(1);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr)); // initializing the values to the serv_addr structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PROXY_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) // binding here
    {
        perror("bind");
        exit(1);
    }
    if (listen(server_fd, 128) < 0) // listening to the port
    {
        perror("listen");
        exit(1);
    }

    while (1)
    {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len); // accepting clients actively
        if (*client_fd < 0)
        {
            perror("accept");
            free(client_fd);
            continue;
        }
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_handler, client_fd);
        pthread_detach(thread_id);
    }
    close(server_fd);
    return 0;
}