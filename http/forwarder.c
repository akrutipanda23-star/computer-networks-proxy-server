#define _POSIX_C_SOURCE 200809L   // enable POSIX networking APIs

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>       // struct addrinfo, getaddrinfo, freeaddrinfo, gai_strerror
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "forwarder.h"

#define BUFFER_SIZE 4096
#define SERVER_TIMEOUT_SECONDS 10

/*
 * Creates a TCP connection to the destination server.
 */
static int connect_to_server(const char *host, int port) {
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    char port_string[16];
    int server_fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    snprintf(port_string, sizeof(port_string), "%d", port);

    int status = getaddrinfo(host, port_string, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_fd < 0) continue;

        if (connect(server_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // success
        }

        close(server_fd);
        server_fd = -1;
    }

    freeaddrinfo(result);
    return server_fd;
}

/*
 * Sends the complete HTTP request to the destination server.
 */
static int send_all(int socket_fd, const char *data, size_t length) {
    size_t total_sent = 0;
    while (total_sent < length) {
        ssize_t bytes_sent = send(socket_fd, data + total_sent, length - total_sent, 0);
        if (bytes_sent < 0) {
            if (errno == EINTR) continue;
            perror("send");
            return -1;
        }
        if (bytes_sent == 0) return -1;
        total_sent += (size_t)bytes_sent;
    }
    return 0;
}

/*
 * Rebuilds the HTTP request using origin-form.
 */
static int build_forward_request(const HttpRequest *request, char *output, size_t output_size) {
    int written = snprintf(output, output_size,
        "%s %s %s\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        request->method,
        request->path,
        request->version,
        request->host
    );

    if (written < 0 || (size_t)written >= output_size) return -1;
    return written;
}

/*
 * Forwards an HTTP request to the destination server
 * and sends the response back to the client.
 */
int forward_http_request(const HttpRequest *request, const char *original_request, int client_fd) {
    int server_fd;
    char forward_request[8192];
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    (void)original_request; // unused

    if (request == NULL) return -1;

    printf("[FORWARD] Connecting to %s:%d\n", request->host, request->port);

    server_fd = connect_to_server(request->host, request->port);
    if (server_fd < 0) {
        fprintf(stderr, "[FORWARD] Could not connect to %s:%d\n", request->host, request->port);
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec  = SERVER_TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    if (build_forward_request(request, forward_request, sizeof(forward_request)) < 0) {
        fprintf(stderr, "[FORWARD] Request too large.\n");
        close(server_fd);
        return -1;
    }

    printf("[FORWARD] Sending request:\n%s", forward_request);

    if (send_all(server_fd, forward_request, strlen(forward_request)) < 0) {
        close(server_fd);
        return -1;
    }

    while (1) {
        bytes_received = recv(server_fd, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "[FORWARD] Server receive timeout.\n");
            } else {
                perror("recv");
            }
            close(server_fd);
            return -1;
        }
        if (bytes_received == 0) break; // server closed connection

        if (send_all(client_fd, buffer, (size_t)bytes_received) < 0) {
            close(server_fd);
            return -1;
        }
    }

    close(server_fd);
    printf("[FORWARD] Response successfully sent to client.\n");
    return 0;
}
