// MIT License
//
// Copyright (c) 2025 Catmengi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



/**
 * poll_network.h - Interface for network handling based on poll
 */

#ifndef POLL_NETWORK_H
#define POLL_NETWORK_H

#include <stdint.h>
#include <sys/socket.h>

/**
 * Minimum size for poll file descriptor array
 */
#define MIN_POLLFD 16

/**
 * Connection structure holding socket connection details
 */
struct poll_connection {
    socklen_t sockaddr_len;
    struct sockaddr* sockaddr;
    int sockfd;
};

/**
 * Callback functions for network events
 */
struct poll_net_callbacks {
    void (*accept_error_cb)(void* ctx);       /* Called when accept fails */
    void (*accept_cb)(int fd, void* ctx);     /* Called when new connection is accepted */
    void (*disconnect_cb)(int fd, void* ctx); /* Called when connection is closed */
    void (*read_cb)(int fd, void* ctx);       /* Called when data is available to read */
};

/**
 * Opaque structure for poll network instance
 */
typedef struct poll_net *poll_net_t;

/**
 * Initialize poll network interface
 * @param port Port number
 * @param cbs Callback functions
 * @param cb_ctx Context pointer passed to callbacks
 * @param connection Socket connection structure
 * @return Poll network handle or NULL on failure
 */
poll_net_t poll_net_init(uint16_t port, struct poll_net_callbacks cbs, void* cb_ctx, struct poll_connection connection);

/**
 * Start accepting connections (creates accept thread)
 * @param net Poll network handle
 */
void poll_net_start_accept(poll_net_t net);

/**
 * Add file descriptor to poll set
 * @param net Poll network handle
 * @param fd File descriptor to add
 */
void poll_net_add_fd(poll_net_t net, int fd);

/**
 * Free poll network resources
 * @param net Poll network handle
 * @param connection_free_cb Callback to free connection resources
 * @param free_ctx Context for free callback
 */
void poll_net_free(poll_net_t net, void (*connection_free_cb)(struct poll_connection con, void* free_ctx), void* free_ctx);

#endif /* POLL_NETWORK_H */
