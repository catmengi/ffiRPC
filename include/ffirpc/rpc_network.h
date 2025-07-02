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

#pragma once

#include <ffirpc/rpc_struct.h>

typedef struct _rpc_net_person* rpc_net_person_t;
typedef struct _rpc_net_holder* rpc_net_holder_t;

typedef struct {
    void* userdata;
    void (*notify)(rpc_net_person_t person, void* userdata);
    void (*persondata_init)(rpc_net_person_t person, void* userdata);
    void (*persondata_destroy)(rpc_net_person_t person, void* userdata);
}rpc_net_notifier_callback;


rpc_net_holder_t rpc_net_holder_create(rpc_net_notifier_callback notifier);
void rpc_net_holder_free(rpc_net_holder_t holder);

void rpc_net_holder_accept_on(rpc_net_holder_t holder, int accept_fd);
void rpc_net_holder_add_fd(rpc_net_holder_t holder, int fd);
rpc_net_notifier_callback rpc_net_holder_get_notify(rpc_net_holder_t holder);

int create_tcp_listenfd(short port);

int rpc_net_send(int fd, rpc_struct_t tosend);
rpc_struct_t rpc_net_recv(int fd);

rpc_struct_t rpc_net_person_get_request(rpc_net_person_t person);
char* rpc_net_person_id(rpc_net_person_t person);
size_t rpc_net_person_request_ammount(rpc_net_person_t person);
int rpc_net_person_fd(rpc_net_person_t person);
