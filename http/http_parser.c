#define _POSIX_C_SOURCE 200809L   // must be defined before including <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // strtok_r is declared here when POSIX_C_SOURCE is set
#include <strings.h>  // strncasecmp is declared here
#include <ctype.h>
#include "http_parser.h"

#define REQUEST_LINE_SIZE 4096

/*
 * Removes leading and trailing whitespace from a string.
 */
static void trim_whitespace(char *str)
{
    char *start;
    char *end;

    if (str == NULL || *str == '\0') {
        return;
    }

    start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

/*
 * Extracts the host and optional port from a Host header.
 */
static int parse_host_header(const char *host_value, HttpRequest *parsed_request)
{
    char host_copy[MAX_HOST_SIZE];
    char *colon;

    if (host_value == NULL || parsed_request == NULL) {
        return -1;
    }

    strncpy(host_copy, host_value, sizeof(host_copy) - 1);
    host_copy[sizeof(host_copy) - 1] = '\0';

    trim_whitespace(host_copy);

    if (strlen(host_copy) == 0) {
        return -1;
    }

    parsed_request->port = 80; // default HTTP port

    colon = strrchr(host_copy, ':');
    if (colon != NULL) {
        char *port_string = colon + 1;
        int valid_port = 1;

        if (*port_string == '\0') {
            valid_port = 0;
        }

        for (char *p = port_string; *p != '\0'; p++) {
            if (!isdigit((unsigned char)*p)) {
                valid_port = 0;
                break;
            }
        }

        if (valid_port) {
            int port = atoi(port_string);
            if (port < 1 || port > 65535) {
                return -1;
            }
            parsed_request->port = port;
            *colon = '\0';
        }
    }

    strncpy(parsed_request->host, host_copy, sizeof(parsed_request->host) - 1);
    parsed_request->host[sizeof(parsed_request->host) - 1] = '\0';

    return 0;
}

/*
 * Parses an HTTP request.
 */
int parse_http_request(const char *request, HttpRequest *parsed_request)
{
    char *request_copy;
    char *line;
    char *saveptr;
    char *method;
    char *url;
    char *version;

    if (request == NULL || parsed_request == NULL) {
        return -1;
    }

    memset(parsed_request, 0, sizeof(HttpRequest));

    request_copy = malloc(strlen(request) + 1);
    if (request_copy == NULL) {
        return -1;
    }
    strcpy(request_copy, request);

    line = strtok_r(request_copy, "\r\n", &saveptr);
    if (line == NULL) {
        free(request_copy);
        return -1;
    }

    method = strtok(line, " \t");
    url = strtok(NULL, " \t");
    version = strtok(NULL, " \t");

    if (method == NULL || url == NULL || version == NULL) {
        free(request_copy);
        return -1;
    }

    strncpy(parsed_request->method, method, sizeof(parsed_request->method) - 1);
    strncpy(parsed_request->version, version, sizeof(parsed_request->version) - 1);

    if (strncmp(url, "http://", 7) == 0) {
        char url_copy[MAX_PATH_SIZE + MAX_HOST_SIZE];
        strncpy(url_copy, url + 7, sizeof(url_copy) - 1);
        url_copy[sizeof(url_copy) - 1] = '\0';

        char *path_start = strchr(url_copy, '/');
        if (path_start != NULL) {
            strncpy(parsed_request->path, path_start, sizeof(parsed_request->path) - 1);
            parsed_request->path[sizeof(parsed_request->path) - 1] = '\0';
            *path_start = '\0';
        } else {
            strcpy(parsed_request->path, "/");
        }

        if (parse_host_header(url_copy, parsed_request) != 0) {
            free(request_copy);
            return -1;
        }
    } else {
        strncpy(parsed_request->path, url, sizeof(parsed_request->path) - 1);
        parsed_request->path[sizeof(parsed_request->path) - 1] = '\0';
    }

    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        if (strncasecmp(line, "Host:", 5) == 0) {
            char *host_value = line + 5;
            trim_whitespace(host_value);
            if (parse_host_header(host_value, parsed_request) != 0) {
                free(request_copy);
                return -1;
            }
            break;
        }
    }

    free(request_copy);

    if (parsed_request->host[0] == '\0') {
        return -1;
    }
    if (parsed_request->path[0] == '\0') {
        strcpy(parsed_request->path, "/");
    }

    return 0;
}
