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




#include "../include/rpc_network.h"
#include "../include/rpc_struct.h"
#include "../include/sc_queue.h"
#include "../include/poll_network.h"

#include <jansson.h>

#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#define HOLDER_MIN_ALLOC_FDS 128
#define HOLDER_LISTEN 1024

static void net_accept(int fd, void* ctx);
static void net_discon(int fd, void* ctx);
static void net_read(int fd, void* ctx);

static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER; //TODO: remove this peace of junk!
static rpc_net_person_t* RN_persons = NULL;
static atomic_int RN_alloced_personsfd = 0;;

struct _rpc_net_person{
    char ID[RPC_STRUCT_ID_SIZE];

    struct sc_queue_ptr request_que;

    int fd; //TODO: windows
};

struct _rpc_net_holder{
    poll_net_t poll_net; //TODO: windows
    rpc_net_notifier_callback notify;
};

rpc_net_holder_t rpc_net_holder_create(rpc_net_notifier_callback notifier){
    rpc_net_holder_t holder = malloc(sizeof(*holder)); assert(holder);

    struct poll_net_callbacks cbs = {
        .accept_cb = net_accept,
        .read_cb = net_read,
        .disconnect_cb = net_discon,
        .accept_error_cb = NULL, //NULLs are not called
    };

    holder->poll_net = poll_net_init(cbs, holder);
    holder->notify = notifier;

    return holder;
}

void rpc_net_holder_free(rpc_net_holder_t holder){
    if(holder){
        poll_net_free(holder->poll_net);
        free(holder);
    }
}

void rpc_net_holder_accept_on(rpc_net_holder_t holder, int accept_fd){
    poll_net_start_accept(holder->poll_net, accept_fd);
}

void rpc_net_holder_add_fd(rpc_net_holder_t holder, int fd){
    poll_net_add_fd(holder->poll_net,fd);
    net_accept(fd, holder); //why not?
}

int create_tcp_listenfd(short port){
    int sockfd = socket(AF_INET,SOCK_STREAM,0); assert(sockfd);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };
    int opt = 1;
    assert(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == 0);

    assert(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    assert(listen(sockfd,HOLDER_LISTEN) == 0);

    return sockfd;
}

static void make_personsfd_bigger(rpc_net_holder_t holder, int to){
    if(to >= RN_alloced_personsfd){  //TODO: proper alloc
        int prev_s = RN_alloced_personsfd;
        RN_alloced_personsfd += HOLDER_MIN_ALLOC_FDS;
        assert((RN_persons = realloc(RN_persons, RN_alloced_personsfd * sizeof(*RN_persons))));

        memset(&RN_persons[prev_s],0,(RN_alloced_personsfd - prev_s) * sizeof(RN_persons));
    }
}

static void net_accept(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    pthread_mutex_lock(&global_lock);
    make_personsfd_bigger(holder,fd); //it have internal check of size, dont worry!

    if(RN_persons[fd] != NULL) {
        pthread_mutex_unlock(&global_lock);
        net_discon(fd,ctx);
        pthread_mutex_lock(&global_lock);
    }

    RN_persons[fd] = malloc(sizeof(*RN_persons[0])); assert(RN_persons[fd]);
    RN_persons[fd]->fd = fd;

    arc4random_buf(RN_persons[fd]->ID,RPC_STRUCT_ID_SIZE - 1);
    RN_persons[fd]->ID[RPC_STRUCT_ID_SIZE - 1] = '\0';

    for(int i = 0 ; i < RPC_STRUCT_ID_SIZE - 1; i++){
        while(RN_persons[fd]->ID[i] == '\0') RN_persons[fd]->ID[i] = arc4random();
        RN_persons[fd]->ID[i] = ID_alphabet[RN_persons[fd]->ID[i] % (sizeof(ID_alphabet) - 1)];
    }

    sc_queue_init(&RN_persons[fd]->request_que);

    if(holder->notify.persondata_init) holder->notify.persondata_init(RN_persons[fd], holder->notify.userdata);

    pthread_mutex_unlock(&global_lock);
}

static void net_discon(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    pthread_mutex_lock(&global_lock);
    if(RN_persons[fd]){
        if(holder->notify.persondata_destroy) holder->notify.persondata_destroy(RN_persons[fd],holder->notify.userdata);

        RN_persons[fd]->fd = 0;
        for(size_t i = 0; i < rpc_net_person_request_ammount(RN_persons[fd]); i++){
            rpc_struct_free(sc_queue_del_first(&RN_persons[fd]->request_que));
        }
        sc_queue_term(&RN_persons[fd]->request_que);

        free(RN_persons[fd]);
        RN_persons[fd] = NULL;
    }
    pthread_mutex_unlock(&global_lock);
}

int rpc_net_send_json(int fd, json_t* json){
    int ret = 1;
    char* send_string = json_dumps(json,JSON_COMPACT);
    if(send_string){
        uint64_t send_len = strlen(send_string);
        if(send(fd,&send_len,sizeof(send_len), MSG_NOSIGNAL) != sizeof(send_len))  goto exit;
        if(send(fd,send_string,(size_t)send_len,MSG_NOSIGNAL) != (size_t)send_len) goto exit;

        ret = 0;
    }
exit:
    free(send_string);
    return ret;
}
static json_t* rpc_net_recv_json(int fd){
    uint64_t recv_len = 0;
    json_t* ret = NULL;
    char* recv_buf = NULL;

    if(recv(fd,&recv_len,sizeof(recv_len),MSG_NOSIGNAL) != sizeof(recv_len)) goto exit;

    recv_buf = malloc(recv_len);
    if(recv_buf == NULL) goto exit;

    if(recv(fd,recv_buf,(size_t)recv_len,MSG_NOSIGNAL) != (size_t)recv_len) goto exit;

    ret = json_loadb(recv_buf,(size_t)recv_len,JSON_DISABLE_EOF_CHECK,NULL);
exit:
    free(recv_buf);
    return ret;
}

static void net_read(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    pthread_mutex_lock(&global_lock);

    if(fd < RN_alloced_personsfd){
    rpc_net_person_t person = RN_persons[fd];
        if(person){

            rpc_struct_t req = rpc_net_recv(fd);
            if(req){
                sc_queue_add_last(&person->request_que,req);
                if(holder->notify.notify) holder->notify.notify(person,holder->notify.userdata);
            } else {shutdown(fd, SHUT_RDWR); close(fd);}
        } else {shutdown(fd, SHUT_RDWR); close(fd);}
    } else {
        printf("%s :: %s:%d fd is BIGGER than RN_alloced_personsfd, BUG!\n",__PRETTY_FUNCTION__, __FILE__, __LINE__);
        shutdown(fd, SHUT_RDWR);
        close(fd);
    } //BUG: Why? Something is really WRONG HERE!

    pthread_mutex_unlock(&global_lock);
}


int rpc_net_send(int fd, rpc_struct_t tosend){
    json_t* send = rpc_struct_serialize(tosend);
    int ret = rpc_net_send_json(fd,send);

    rpc_struct_free(tosend); //dont need it now, should be it free now
    json_decref(send);

    return ret;
}

rpc_struct_t rpc_net_recv(int fd){
    json_t* json = rpc_net_recv_json(fd);
    rpc_struct_t reply = NULL;
    if(json){
        reply = rpc_struct_deserialize(json);
        json_decref(json);
    }
    return reply;
}

rpc_struct_t rpc_net_person_get_request(rpc_net_person_t person){

    rpc_struct_t request = NULL;
    if(person && rpc_net_person_request_ammount(person) > 0){
        request = sc_queue_del_first(&person->request_que);
    }

    return request;
}

char* rpc_net_person_id(rpc_net_person_t person){
    char* ret = NULL;
    if(person) ret = person->ID;
    return ret;
}

size_t rpc_net_person_request_ammount(rpc_net_person_t person){
    assert(person);
    return sc_queue_size(&person->request_que);
}
rpc_net_notifier_callback rpc_net_holder_get_notify(rpc_net_holder_t holder){
    if(holder)
        return holder->notify;

    return (rpc_net_notifier_callback){0};
}
int rpc_net_person_fd(rpc_net_person_t person){
    assert(person);

    int fd = person->fd;

    return fd;
}
