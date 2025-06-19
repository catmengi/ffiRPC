#include "../include/rpc_network.h"
#include "../include/rpc_struct.h"
#include "../include/sc_queue.h"
#include "../include/poll_network.h"

#include <jansson.h>

#include <pthread.h>
#include <assert.h>
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
    rpc_struct_t persondata;

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
    pthread_mutex_lock(&global_lock);

    if(to >= RN_alloced_personsfd){  //TODO: proper alloc
        int prev_s = RN_alloced_personsfd;
        RN_alloced_personsfd += HOLDER_MIN_ALLOC_FDS;
        assert((RN_persons = realloc(RN_persons, RN_alloced_personsfd * sizeof(*RN_persons))));

        memset(&RN_persons[prev_s],0,RN_alloced_personsfd - prev_s * sizeof(RN_persons));
    }

    pthread_mutex_unlock(&global_lock);
}

static void net_accept(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    make_personsfd_bigger(holder,fd); //it have internal check of size, dont worry!

    pthread_mutex_lock(&global_lock);

    RN_persons[fd] = malloc(sizeof(*RN_persons[0])); assert(RN_persons[fd]);
    RN_persons[fd]->fd = fd;
    RN_persons[fd]->persondata = rpc_struct_create();
    sc_queue_init(&RN_persons[fd]->request_que);

    if(holder->notify.persondata_init) holder->notify.persondata_init(RN_persons[fd], holder->notify.userdata);

    pthread_mutex_unlock(&global_lock);
}

static void net_discon(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    pthread_mutex_lock(&global_lock);
    if(RN_persons[fd]){
        RN_persons[fd]->fd = 0;
        for(size_t i = 0; i < rpc_net_person_request_ammount(RN_persons[fd]); i++){
            rpc_struct_free(sc_queue_del_first(&RN_persons[fd]->request_que));
        }
        sc_queue_term(&RN_persons[fd]->request_que);
        rpc_struct_free(RN_persons[fd]->persondata);

        free(RN_persons[fd]);
        RN_persons[fd] = NULL;
    }
    pthread_mutex_unlock(&global_lock);
}

static void net_read(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    pthread_mutex_lock(&global_lock);

    assert(RN_persons[fd]); //ASSERT!
    rpc_net_person_t person = RN_persons[fd];

    json_t* req_json = json_loadfd(fd,JSON_DISABLE_EOF_CHECK | JSON_DECODE_ANY, NULL);

    rpc_struct_t req = rpc_struct_unserialise(req_json); //TODO: callback based read to use encryption and compression
    if(req){
        sc_queue_add_last(&person->request_que,req);
        if(holder->notify.notify) holder->notify.notify(person,holder->notify.userdata);
    } else {shutdown(fd, SHUT_RDWR); close(fd);}

    json_decref(req_json);

    pthread_mutex_unlock(&global_lock);

}

int rpc_net_send(int fd, rpc_struct_t tosend){
    json_t* send = rpc_struct_serialise(tosend);
    int ret = json_dumpfd(send,fd,JSON_COMPACT);

    if(ret == 0) rpc_struct_free(tosend); //dont need it now, should be it free now
    json_decref(send);

    return ret;
}

rpc_struct_t rpc_net_person_get_request(rpc_net_person_t person){

    rpc_struct_t request = NULL;
    if(person && rpc_net_person_request_ammount(person) > 0){
        request = sc_queue_del_first(&person->request_que);
    }

    return request;
}
rpc_struct_t rpc_net_persondata(rpc_net_person_t person){
    return person == NULL ? NULL : person->persondata;
}
size_t rpc_net_person_request_ammount(rpc_net_person_t person){
    assert(person);
    return sc_queue_size(&person->request_que);
}
int rpc_net_person_fd(rpc_net_person_t person){
    assert(person);

    int fd = person->fd;

    return fd;
}
