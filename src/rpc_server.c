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



#include "../include/rpc_server.h"
#include "../include/rpc_thread_context.h"
#include "../include/poll_network.h"
#include "../include/rpc_protocol.h"
#include "../include/sc_queue.h"
#include "../include/C-Thread-Pool/thpool.h"

#include <assert.h>
#include <ffi.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define RPC_SERVER_ALLOC_MIN_PORTS 16
#define RPC_SERVER_LISTEN_BACKLOG 1024

ffi_type* rpctype_to_libffi[RPC_duplicate] = {   //convert table used to convert from rpc_types to ffi type
    &ffi_type_void,
    &ffi_type_schar,
    &ffi_type_uint8, &ffi_type_sint16,
    &ffi_type_uint16, &ffi_type_sint32,
    &ffi_type_uint32, &ffi_type_sint64,
    &ffi_type_uint64, &ffi_type_double,

    &ffi_type_pointer, &ffi_type_pointer,
    &ffi_type_pointer, &ffi_type_pointer
};

enum rpc_server_state{
    SERVER_BEFORE_AUTH, //server before auth mode, only RPC_AUTH, RPC_DISCONNNECT, RPC_PING should be allowed
    SERVER_NORMAL //Set's server processing mode to allow all commands
};

typedef struct rpc_function{
    rpc_struct_t function_context;

    char* function_name;
    void* function_ptr;

    enum rpc_types return_type;

    enum rpc_types* prototype;
    int prototype_len;
}*rpc_function_t;

static struct rpc_server{
    threadpool execution_pool;

    rpc_struct_t functions;

    rpc_struct_t users;
    rpc_struct_t fd_users_map;

    size_t network_size;
    size_t network_index;
    poll_net_t* network;

    struct poll_net_callbacks network_cbs;

}rpc_server;

void rpc_server_init(){
    rpc_server.execution_pool = thpool_init((int)sysconf(_SC_NPROCESSORS_ONLN));

    rpc_server.functions = rpc_struct_create();
    rpc_server.users = rpc_struct_create();
    rpc_server.fd_users_map = rpc_struct_create();

    rpc_server.network_size = RPC_SERVER_ALLOC_MIN_PORTS;
    rpc_server.network_index = 0;
    rpc_server.network = malloc(rpc_server.network_size * sizeof(*rpc_server.network)); assert(rpc_server.network);
}

__attribute__((destructor))
void rpc_server_deinit(){
    for(size_t i = 0; i < rpc_server.network_index; i++){
        poll_net_stop_accept(rpc_server.network[i]);
    }
    thpool_wait(rpc_server.execution_pool);
    thpool_destroy(rpc_server.execution_pool);

    char** FN_keys = rpc_struct_keys(rpc_server.functions);
    for(size_t i = 0; i < rpc_struct_length(rpc_server.functions); i++){
        rpc_server_remove_function(FN_keys[i]);
    }
    free(FN_keys);
    rpc_struct_free(rpc_server.functions);

    char** USERS_keys = rpc_struct_keys(rpc_server.users);
    for(size_t i = 0; i < rpc_struct_length(rpc_server.users); i++){
        //do something
    }
    free(USERS_keys);
    rpc_struct_free(rpc_server.users);

    char** FD_USERS_keys = rpc_struct_keys(rpc_server.fd_users_map);
    for(size_t i = 0; i < rpc_struct_length(rpc_server.fd_users_map); i++){
        //do something
    }
    free(FD_USERS_keys);
    rpc_struct_free(rpc_server.fd_users_map);
    for(size_t i = 0; i < rpc_server.network_index; i++){
        poll_net_free(rpc_server.network[i]);
    }
    free(rpc_server.network);
    memset(&rpc_server,0,sizeof(rpc_server));
}

int rpc_server_add_function(char* function_name, void* function_ptr,enum rpc_types return_type, enum rpc_types* prototype, int prototype_len){
    assert(function_name); assert(function_ptr);

    rpc_function_t function = NULL;
    if(rpc_struct_get(rpc_server.functions,function_name,function) != 0){
        function = calloc(1,sizeof(*function)); assert(function);

        function->function_context = rpc_struct_create();
        function->function_ptr = function_ptr;

        function->function_name = strdup(function_name); assert(function->function_name);

        function->prototype_len = prototype_len;
        function->prototype = malloc(sizeof(*prototype) * prototype_len); assert(function->prototype);
        memcpy(function->prototype,prototype,sizeof(*prototype) * prototype_len);

        function->return_type = return_type;

        assert(rpc_struct_set(rpc_server.functions,function_name,function) == 0);
        return ERR_RPC_OK;
    }
    return ERR_RPC_FUNCTION_EXIST;
}

void rpc_server_remove_function(char* function_name){
    rpc_function_t function = NULL;
    if(rpc_struct_get(rpc_server.functions,function_name,function) == 0){
        assert(rpc_struct_remove(rpc_server.functions,function_name) == 0);

        rpc_struct_free(function->function_context);
        free(function->function_name);
        free(function->prototype);
        free(function);
    }
}

struct rpc_updated_argument_info{
    char key[sizeof(int) * 4];
    uint64_t hash;

    enum rpc_types type;
    void* raw_ptr;
    int index;
};

int rpc_server_compare_protos(rpc_struct_t arguments, enum rpc_types* prototype, int prototype_len){
    int ret = 0;
    char** keys = NULL;

    if(arguments == NULL && prototype == NULL){ret = 1; goto exit;}
    if((arguments != NULL && prototype == NULL) || (arguments == NULL && prototype != NULL)) goto exit;

    if(rpc_struct_length(arguments) < prototype_len) goto exit; //variadic functions quirk

    keys = rpc_struct_keys(arguments);
    for(int i = 0; i < prototype_len; i++){
        if(rpc_struct_typeof(arguments,keys[i]) != prototype[i]) goto exit;
    }
    ret = 1;

exit:
    free(keys);
    return ret;
}
int rpc_server_is_variadic(rpc_struct_t arguments, int prototype_len){
    return rpc_struct_length(arguments) > prototype_len;
}
int rpc_server_is_variadic_allowed(rpc_struct_t arguments, int variadic_start){
    int is_allowed = 1;
    char** keys = rpc_struct_keys(arguments);
    for(int i = 0; i < rpc_struct_length(arguments); i++){
        enum rpc_types type = rpc_struct_typeof(arguments,keys[i]);
        if(type < ctype_to_rpc(int)){
            is_allowed = 0;
            goto exit;
        }
    }
exit:
    free(keys);
    return is_allowed;
}

int rpc_server_call(rpc_function_t function, rpc_struct_t arguments, rpc_struct_t output){
    char key[sizeof(int) * 4];
    if(!rpc_server_compare_protos(arguments,function->prototype,function->prototype_len)) return ERR_RPC_PROTOTYPE_DIFFERENT;

     //libffi NOT allow types smaller than int exist in variadic arguments....
     if(rpc_server_is_variadic(arguments,function->prototype_len) && rpc_server_is_variadic_allowed(arguments,function->prototype_len) == 0) return ERR_RPC_VARIADIC_NOT_ALLOWED;


    ffi_cif cif;
    ffi_type** ffi_prototype = malloc(sizeof(*ffi_prototype) * rpc_struct_length(arguments)); assert(ffi_prototype);

    for(int i = 0; i < rpc_struct_length(arguments); i++){
        sprintf(key,"%d",i);
        ffi_prototype[i] = rpctype_to_libffi[rpc_struct_typeof(arguments,key)];
    }
    assert(ffi_prep_cif_var(&cif,FFI_DEFAULT_ABI,function->prototype_len,(int)rpc_struct_length(arguments),rpctype_to_libffi[function->return_type],ffi_prototype) == 0);
    void** ffi_arguments = calloc(rpc_struct_length(arguments),sizeof(void*)); assert(ffi_arguments);

    struct sc_queue_ptr updated_arguments;
    sc_queue_init(&updated_arguments);

    for(int i = 0; i < (int)rpc_struct_length(arguments); i++){
        sprintf(key,"%d",i);
        ffi_arguments[i] = calloc(1,sizeof(uint64_t)); assert(ffi_arguments[i]);
        rpc_struct_get_unsafe(arguments,key,*(uint64_t*)ffi_arguments[i]); //using unsafe, because type safe variant will crash us out

        enum rpc_types type = rpc_struct_typeof(arguments,key);

        if(rpc_is_pointer(type) && type != RPC_unknown){
            struct rpc_updated_argument_info* arg_info = calloc(1,sizeof(*arg_info)); assert(arg_info);

            arg_info->index = i;
            arg_info->type = type;
            arg_info->raw_ptr = *(void**)ffi_arguments[i];
            strcpy(arg_info->key,key);

            switch(type){ //calculating item's hash to decide to set them in repacked or not
                case RPC_struct:
                    arg_info->hash = rpc_struct_hash(*(void**)ffi_arguments[i]);
                    break;
                case RPC_sizedbuf:
                    arg_info->hash = rpc_sizedbuf_hash(*(void**)ffi_arguments[i]);
                    break;
                case RPC_string:
                    arg_info->hash = murmur((uint8_t*)*(void**)ffi_arguments[i],strlen(*(char**)ffi_arguments[i]));
                    break;

                default: break; //unhasheble type?
            }
            sc_queue_add_last(&updated_arguments,arg_info);
        }
    }
    rpc_struct_t context = rpc_thread_context_get(); //get thread execution context
    rpc_struct_set(context, "function_context", function->function_context);

    void* return_is = NULL;
    uint64_t ffi_return = 0;

    ffi_call(&cif,function->function_ptr,&ffi_return,ffi_arguments);
    free(ffi_prototype);

    for(int i = 0; i < (int)rpc_struct_length(arguments); i++)
        free(ffi_arguments[i]);
    free(ffi_arguments);

    int updated_arguments_len = sc_queue_size(&updated_arguments);
    for(int i = 0; i < updated_arguments_len; i++){
        struct rpc_updated_argument_info* arg_info = sc_queue_del_first(&updated_arguments);
        assert(arg_info);

        int do_set = 0;
        if(arg_info->raw_ptr == (void*)ffi_return){
            return_is = arg_info->raw_ptr;
            do_set = 1;
        }
        switch(arg_info->type){
            case RPC_struct:
                if(rpc_struct_hash(arg_info->raw_ptr) != arg_info->hash) do_set = 1; //this means value have been changed
                break;
            case RPC_sizedbuf:
                if(rpc_sizedbuf_hash(arg_info->raw_ptr) != arg_info->hash) do_set = 1;
                break;
            case RPC_string:
                if(murmur(arg_info->raw_ptr,strlen(arg_info->raw_ptr)) != arg_info->hash) do_set = 1;
                break;
            default: break;
        }

        if(do_set == 1){
            struct rpc_container_element* element = malloc(sizeof(*element));
            element->type = arg_info->type;
            element->data = arg_info->raw_ptr;
            rpc_struct_set_internal(output,arg_info->key,element);
        }
        free(arg_info);
        continue;
    }
    sc_queue_term(&updated_arguments);

    if(function->return_type != RPC_none && function->return_type != RPC_unknown){ //we cannot serialise RPC_unknown since it is a raw pointer
        if(!(rpc_is_pointer(function->return_type) && (void*)ffi_return == NULL)){
            struct rpc_container_element* element = calloc(1,sizeof(*element)); assert(element);
            element->type = function->return_type;
            element->length = rpctype_sizes[element->type];
            if(rpc_is_pointer(element->type)){
                element->data = (void*)ffi_return;
            } else {
                element->data = malloc(element->length); assert(element->data);
                memcpy(element->data,&ffi_return,element->length);
            }
            rpc_struct_set_internal(output,"return",element);
            if(function->return_type == RPC_string) free((char*)ffi_return); //can free, because in rpc_struct_set_internal in was strdup-ed
        }
    }
    return ERR_RPC_OK;
}

void rpc_server_launch_port(uint16_t port){
    if(rpc_server.network_index == rpc_server.network_size - 1){
        rpc_server.network_size += RPC_SERVER_ALLOC_MIN_PORTS;
        assert((rpc_server.network = realloc(rpc_server.network,rpc_server.network_size * sizeof(*rpc_server.network))) != NULL);
    }
    int net_index = rpc_server.network_index++;
    rpc_server.network[net_index] = poll_net_init(rpc_server.network_cbs,NULL);

    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    assert(listen_fd >= 0);
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int));
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    assert(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)) == 0);
    assert(listen(listen_fd,RPC_SERVER_LISTEN_BACKLOG) == 0);

    poll_net_start_accept(rpc_server.network[net_index],listen_fd); 
}
