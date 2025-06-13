#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/sc_queue.h"
#include "../include/rpc_thread_context.h"

#include <assert.h>
#include <ffi.h>

typedef struct{
    rpc_struct_t lobjects; //client's local objects, accessed by RS_callable_objects's object ID

    //space for future items
}*rpc_client_ctx_t;


static rpc_struct_t RS_shared_objects = NULL; //named objects that can store any kind of data
static rpc_struct_t RS_callable_objects = NULL; //this structure will store rpc_struct_t with rpc_struct_t with client_specific objects and rpc_struct_t with callable functions

void rpc_server_init(){
    RS_shared_objects = rpc_struct_create();
    RS_callable_objects = rpc_struct_create();
}

int rpc_server_add_shared_object(char* name, rpc_struct_t obj){
    return rpc_struct_set(RS_shared_objects, name, obj);
}
rpc_struct_t rpc_server_get_shared_object(char* name){
    rpc_struct_t obj = NULL;
    if(name){
        rpc_struct_get(RS_shared_objects, name, obj);
    }
    return obj;
}

int rpc_server_create_callable_object(char* name){
    rpc_struct_t new = rpc_struct_create();
    int ret = rpc_struct_set(RS_callable_objects, name,new);

    if(ret == 0){
        assert(rpc_struct_set(new, "local_objects", rpc_struct_create()) == 0);
        assert(rpc_struct_set(new, "functions", rpc_struct_create()) == 0);
    } else rpc_struct_free(new);

    return ret;
}

int rpc_server_callable_object_add_function(char* name, rpc_function_t function){
    rpc_struct_t callable_object = NULL;
    volatile int ret = rpc_struct_get(RS_callable_objects, name, callable_object); //i dont know if compiler can mess something up there?

    if(ret == 0){
        rpc_struct_t objects_functions = NULL;
        ret = rpc_struct_get(callable_object, "functions", objects_functions);

        if(ret == 0){
            ret = rpc_struct_set(objects_functions, name, function);
        }

    }

    return ret;
}
