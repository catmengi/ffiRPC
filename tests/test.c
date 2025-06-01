#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/rpc_init.h"
#include "../include/ptracker.h"
#include "../include/rpc_protocol.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(){
    rpc_init();

    rpc_struct_t s = rpc_struct_create();
    rpc_struct_set(s,"1",123);

    double o;
    assert(rpc_struct_get(s,"1",o) != 0);

    rpc_struct_free(s);
}
