#include "../include/rpc_server.h"
#include "../include/rpc_struct.h"

#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

rpc_struct_t test_wrap(char* name, rpc_struct_t arguments);

int main(){

    enum rpc_types prototype[] = {ctype_to_rpc(char*)};
    rpc_server_add_function("test function",printf,ctype_to_rpc(int),prototype,ARRAY_SIZE(prototype));

    rpc_struct_t args = rpc_struct_create();
    rpc_struct_set(args,"0",(char*)"\n\n\n\n abcd printf test! %s \n\n\n\n");
    rpc_struct_set(args,"1",(char*)"Hello World!");


    test_wrap("test function",args);

    rpc_struct_free(args);

    rpc_struct_t check2 = rpc_struct_create();
    rpc_struct_t check_IN = rpc_struct_create();
    rpc_struct_set(check_IN,"check str",(char*)"abcd");
    rpc_struct_set(check2,"check",(rpc_struct_t)check_IN);

    rpc_struct_unlink(check2,"check");

    rpc_struct_t ACC = NULL;
    rpc_struct_get(check2,"check",ACC);
    assert(ACC);

    rpc_struct_free(check2);
    char* output = NULL;
    rpc_struct_get(ACC,"check str",output);
    assert(output);
    puts(output);
    rpc_struct_free(ACC);
}
//=================
