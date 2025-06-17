#include "rpc_struct.h"

typedef struct _rpc_net_person* rpc_net_person_t;
typedef struct _rpc_net_holder* rpc_net_holder_t;

typedef struct {
    void* userdata;
    void (*notify)(rpc_net_person_t person, void* userdata);
}rpc_net_notifier_callback;


rpc_net_holder_t rpc_net_holder_create(rpc_net_notifier_callback notifier);
void rpc_net_holder_accept_on(rpc_net_holder_t holder, int accept_fd);
void rpc_net_holder_add_fd(rpc_net_holder_t holder, int fd);
int create_tcp_listenfd(short port);

int rpc_net_send(int fd, rpc_struct_t tosend);

rpc_struct_t rpc_net_person_get_request(rpc_net_person_t person);
rpc_struct_t rpc_net_person_userdata(rpc_net_person_t person);
size_t rpc_net_person_request_ammount(rpc_net_person_t person);
int rpc_net_person_fd(rpc_net_person_t person);
