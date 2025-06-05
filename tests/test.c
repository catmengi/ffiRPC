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

    char* IDo = rpc_struct_id_get(s);

    size_t u = 0;
    char* buf = rpc_struct_serialise(s, &u);

    rpc_struct_t n = rpc_struct_unserialise(buf); free(buf);

    assert(strcmp(IDo, rpc_struct_id_get(n)) == 0);

    puts(rpc_struct_id_get(s));
    puts(rpc_struct_id_get(n));

    rpc_struct_free(s);
    rpc_struct_free(n);
}
