#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define MAX_METHOD_SIZE 16
#define MAX_HOST_SIZE 256
#define MAX_PATH_SIZE 2048
#define MAX_VERSION_SIZE 16
#define MAX_PORT_SIZE 6

typedef struct {
    char method[MAX_METHOD_SIZE];
    char host[MAX_HOST_SIZE];
    char path[MAX_PATH_SIZE];
    char version[MAX_VERSION_SIZE];
    int port;
} HttpRequest;

/*
 * Parses an HTTP request received from a client.
 *
 * Returns:
 *   0  - success
 *  -1  - invalid request
 */
int parse_http_request(const char *request, HttpRequest *parsed_request);

#endif