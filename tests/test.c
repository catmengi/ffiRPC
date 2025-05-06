#include "../include/rpc_server.h"

#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

rpc_struct_t test_wrap(char* name, rpc_server_t server, rpc_struct_t arguments);

int main(){
    rpc_server_t server = rpc_server_create();

    enum rpc_types prototype[1] = {ctype_to_rpc(char*)};
    rpc_server_add_function(server,"test function",printf,ctype_to_rpc(int),prototype,ARRAY_SIZE(prototype));

    rpc_struct_t args = rpc_struct_create();
    rpc_struct_set(args,"0",(char*)"\n\n\n\n abcd printf test! %d \n\n\n\n");
    rpc_struct_set(args,"1",(uint32_t)228);


    test_wrap("test function",server,args);

    rpc_struct_free(args);
    rpc_server_free(server);
}
//=================
