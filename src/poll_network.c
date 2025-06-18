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


#define _GNU_SOURCE

#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/resource.h>

#include "../include/poll_network.h"

#define TIMEOUT 5

struct poll_net{
    int active;

    pthread_t accept_thread;
    pthread_t poll_thread;

    struct pollfd* fds;
    int fds_size; //real allocated size
    nfds_t nfds; //poll() nfds

    void* callback_ctx;
    struct poll_net_callbacks callbacks;

    int sockfd; //should -1 before poll_net_start_accept
};

static void* accept_thread(void* paramP){
    poll_net_t net = paramP;
    while(net->active){
        int netfd = accept(net->sockfd,NULL,NULL);

        if(netfd < 0){
            if(net->callbacks.accept_error_cb && net->active == 1) net->callbacks.accept_error_cb(net->callback_ctx);
            return NULL;
        }
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        assert(setsockopt(netfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) == 0);
        assert(setsockopt(netfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)) == 0);

        poll_net_add_fd(net,netfd);

        if(net->callbacks.accept_cb) net->callbacks.accept_cb(netfd,net->callback_ctx);
    }
    return NULL;
}

static void* poll_thread(void* paramP){
    poll_net_t net = paramP;

    while(net->active){
        int scan = poll(net->fds,net->nfds,15);
        assert(scan >= 0);
        if(scan > 0){
            for(nfds_t i = 0; i < net->nfds; i++){
                if(net->fds[i].revents & POLLIN){
                    if(net->callbacks.read_cb){
                        net->callbacks.read_cb(net->fds[i].fd, net->callback_ctx);
                    }
                }

                if(net->fds[i].revents & POLLHUP || net->fds[i].revents & POLLERR || net->fds[i].revents & POLLNVAL || net->fds[i].revents & POLLRDHUP){
                    if(net->callbacks.disconnect_cb){
                        net->callbacks.disconnect_cb(net->fds[i].fd, net->callback_ctx);
                    }

                    shutdown(net->fds[i].fd,SHUT_RDWR); //just to be sure poll_net will shutdown this socket on another side
                    close(net->fds[i].fd); //just to be sure that it is closed
                    memmove(&net->fds[i],&net->fds[i + 1],sizeof(*net->fds) * (net->nfds - (i + 1)));
                    net->nfds--; i--;
                }
            }
        }
    }
    return NULL;
}

poll_net_t poll_net_init(struct poll_net_callbacks cbs, void* cb_ctx){
    poll_net_t net = calloc(1,sizeof(*net)); assert(net);

    net->active = 1;
    net->callback_ctx = cb_ctx;
    net->callbacks = cbs;
    net->sockfd = -1;

    net->fds_size = MIN_POLLFD;
    net->nfds = 0;
    assert((net->fds = calloc(net->fds_size,sizeof(*net->fds))));

    net->accept_thread = 0;
    assert(pthread_create(&net->poll_thread,NULL,poll_thread,net) == 0);

    return net;
}

void poll_net_start_accept(poll_net_t net, int sockfd){
    net->sockfd = sockfd;
    assert(pthread_create(&net->accept_thread,NULL,accept_thread,net) == 0);
}
int poll_net_add_fd(poll_net_t net, int fd){
    assert(net);
    assert(fd >= 0);

    if(net->active > 0){
        if(net->nfds == net->fds_size - 1){
            net->fds_size += MIN_POLLFD;
            assert((net->fds = realloc(net->fds,(net->fds_size) * sizeof(*net->fds))));
        }
        if(net->nfds + 1 == RLIMIT_NOFILE){
            shutdown(net->fds[net->nfds].fd,SHUT_RDWR);
            close(net->fds[net->nfds].fd);
            return 1;
        }

        int fd_index = net->nfds++;
        net->fds[fd_index].fd = fd;
        net->fds[fd_index].events = POLLIN | POLLRDHUP;
        net->fds[fd_index].revents = 0;
        return 0;
    }
    return 1;
}

void poll_net_free(poll_net_t net){
    net->active = 0;

    if(net->sockfd != -1){
        shutdown(net->sockfd,SHUT_RDWR);
        close(net->sockfd);
    }

    if(net->accept_thread > 0) pthread_join(net->accept_thread,NULL);
    pthread_join(net->poll_thread,NULL);

    for(nfds_t i = 0; i < net->nfds; i++){
        net->callbacks.disconnect_cb(net->fds[i].fd, net->callback_ctx);
        shutdown(net->fds[i].fd,SHUT_RDWR);
        close(net->fds[i].fd);
    }
    free(net->fds);
    free(net);
}

void poll_net_stop_accept(poll_net_t net){
    if(net->sockfd != -1){
        shutdown(net->sockfd,SHUT_RDWR);
        close(net->sockfd);
    }
    if(net->accept_thread > 0) pthread_join(net->accept_thread,NULL);
    net->accept_thread = 0;
}
