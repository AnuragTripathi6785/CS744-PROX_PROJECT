#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

// defining the port numbers and other macros
#define PROXY_PORT 8080
#define ORIGIN_PORT 8000
#define BUFFER_SIZE 8192

void *client_handler(void *arg)
{
    sleep(5);
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
    printf("Thread %lu handling the request:\n", pthread_self());

    // checking if it's a get request or any other
    char method[8], path[1024];
    sscanf(buffer, "%s %s", method, path);
    printf("Method=%s Path=%s\n", method, path);
    if (strcmp(method, "GET") != 0)
    {
        const char *error_msg = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 22\r\n\r\nOnly GET supported for now!";
        write(client_fd, error_msg, strlen(error_msg));
        close(client_fd);
        return NULL;
    }

    // establishing connection with the origin server
    int origin_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in origin_addr;
    memset(&origin_addr, 0, sizeof(origin_addr));
    origin_addr.sin_family = AF_INET;
    origin_addr.sin_addr.s_addr = INADDR_ANY;
    origin_addr.sin_port = htons(ORIGIN_PORT);
    inet_pton(AF_INET, "127.0.0.1", &origin_addr.sin_addr);

    if (connect(origin_fd, (struct sockaddr *)&origin_addr, sizeof(origin_addr)) < 0)
    {
        perror("connect to origin");
        close(origin_fd);
        close(client_fd);
        return NULL;
    }

    // sending the request from client to origin
    write(origin_fd, buffer, r);

    // reading and fwding reply from origin to the client
    ssize_t n;
    while ((n = read(origin_fd, buffer, sizeof(buffer))) > 0)
    {
        write(client_fd, buffer, n);
    }
    close(origin_fd);
    close(client_fd);
    return NULL;
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
    if (listen(server_fd, 10) < 0) // listening to the port
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