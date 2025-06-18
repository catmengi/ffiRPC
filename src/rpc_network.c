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
#include <unistd.h>

#define HOLDER_MIN_ALLOC_FDS 128
#define HOLDER_LISTEN 1024

static void net_accept(int fd, void* ctx);
static void net_discon(int fd, void* ctx);
static void net_read(int fd, void* ctx);

struct _rpc_net_person{
    rpc_struct_t userdata;
    struct sc_queue_ptr request_que;

    int fd; //TODO: windows
};

struct _rpc_net_holder{
    poll_net_t poll_net; //TODO: windows
    rpc_net_notifier_callback notify;

    rpc_net_person_t* persons;
    int* fds;

    int alloced_personsfd;
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

    holder->alloced_personsfd = HOLDER_MIN_ALLOC_FDS;
    holder->fds = malloc(sizeof(*holder->fds) * holder->alloced_personsfd); assert(holder->fds);
    holder->persons = malloc(sizeof(*holder->persons) *holder->alloced_personsfd); assert(holder->persons);

    return holder;
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
    if(to >= holder->alloced_personsfd){  //TODO: proper alloc
        int prev_s = holder->alloced_personsfd;
        holder->alloced_personsfd += HOLDER_MIN_ALLOC_FDS;
        assert((holder->fds = realloc(holder->fds, holder->alloced_personsfd * sizeof(*holder->fds))));
        assert((holder->persons = realloc(holder->persons, holder->alloced_personsfd * sizeof(*holder->persons))));

        memset(&holder->fds[prev_s],0,holder->alloced_personsfd - prev_s * sizeof(holder->fds));
        memset(&holder->persons[prev_s],0,holder->alloced_personsfd - prev_s * sizeof(holder->persons));
    }
}

static void net_accept(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    make_personsfd_bigger(holder,fd); //it have internal check of size, dont worry!

    holder->fds[fd] = fd;

    holder->persons[fd] = malloc(sizeof(*holder->persons[0])); assert(holder->persons[fd]);

    holder->persons[fd]->fd = fd;
    holder->persons[fd]->userdata = rpc_struct_create();
    sc_queue_init(&holder->persons[fd]->request_que);
}

static void net_discon(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    holder->fds[fd] = 0;

    if(holder->persons[fd]){
        holder->persons[fd]->fd = 0;
        sc_queue_term(&holder->persons[fd]->request_que);
        rpc_struct_free(holder->persons[fd]->userdata);

        free(holder->persons[fd]);
        holder->persons[fd] = NULL;
    }
}

static void net_read(int fd, void* ctx){
    rpc_net_holder_t holder = ctx;

    assert(holder->persons[fd]); //ASSERT!

    rpc_net_person_t person = holder->persons[fd];

    rpc_struct_t req = rpc_struct_unserialise(json_loadfd(fd,JSON_DISABLE_EOF_CHECK | JSON_DECODE_ANY, NULL)); //TODO: callback based read to use encryption and compression
    if(req){
        sc_queue_add_last(&person->request_que,req);
        if(holder->notify.notify) holder->notify.notify(person,holder->notify.userdata);
    } else {shutdown(fd, SHUT_RDWR); close(fd);}


}

int rpc_net_send(int fd, rpc_struct_t tosend){
    json_t* send = rpc_struct_serialise(tosend);
    int ret = json_dumpfd(send,fd,JSON_COMPACT);

    if(ret == 0) rpc_struct_free(tosend); //dont need it now, should be it free now
    json_decref(send);

    return ret;
}

rpc_struct_t rpc_net_person_get_request(rpc_net_person_t person){
    return person == NULL ? NULL : sc_queue_del_first(&person->request_que);
}
rpc_struct_t rpc_net_person_userdata(rpc_net_person_t person){
    return person == NULL ? NULL : person->userdata;
}
size_t rpc_net_person_request_ammount(rpc_net_person_t person){
    assert(person);
    return sc_queue_size(&person->request_que);
}
int rpc_net_person_fd(rpc_net_person_t person){
    return person == NULL ? -1 : person->fd;
}
