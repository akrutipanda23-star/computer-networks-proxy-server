#ifndef FORWARDER_H
#define FORWARDER_H

#include "http_parser.h"

/*
 * Connects to the destination server and forwards
 * the HTTP request.
 *
 * Returns:
 *   0  - success
 *  -1  - connection/forwarding error
 */
int forward_http_request(
    const HttpRequest *request,
    const char *original_request,
    int client_fd
);

#endif