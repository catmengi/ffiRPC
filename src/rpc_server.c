#include "../include/rpc_server.h"
#include "../include/rpc_thread_context.h"
#include "../include/sc_queue.h"

#include <assert.h>
#include <ffi-x86_64.h>
#include <ffi.h>
#include <stdint.h>

enum rpc_server_errors{
    RPC_OK,
    RPC_PROTOTYPE_DIFFERENT,
    RPC_VARIADIC_NOT_ALLOWED,
    RPC_FUNCTION_EXIST,
};

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

typedef struct rpc_function{
    rpc_struct_t function_context;

    char* function_name;
    void* function_ptr;

    enum rpc_types return_type;

    enum rpc_types* prototype;
    int prototype_len;
}*rpc_function_t;

static rpc_struct_t rpc_server_functions = NULL;
static rpc_struct_t rpc_server_users = NULL;

__attribute__((constructor))
void rpc_server_create(){ //should be called once!
    rpc_init_thread_context();
    rpc_server_functions = rpc_struct_create();
    rpc_server_users = rpc_struct_create();
}

int rpc_server_add_function(char* function_name, void* function_ptr,enum rpc_types return_type, enum rpc_types* prototype, int prototype_len){
    assert(function_name); assert(function_ptr);

    rpc_function_t function = NULL;
    if(rpc_struct_get(rpc_server_functions,function_name,function) != 0){
        function = calloc(1,sizeof(*function)); assert(function);

        function->function_context = rpc_struct_create();
        function->function_ptr = function_ptr;

        function->function_name = strdup(function_name); assert(function->function_name);

        function->prototype_len = prototype_len;
        function->prototype = malloc(sizeof(*prototype) * prototype_len); assert(function->prototype);
        memcpy(function->prototype,prototype,sizeof(*prototype) * prototype_len);

        function->return_type = return_type;

        assert(rpc_struct_set(rpc_server_functions,function_name,function) == 0);
        return RPC_OK;
    }
    return RPC_FUNCTION_EXIST;
}

void rpc_server_remove_function(char* function_name){
    rpc_function_t function = NULL;
    if(rpc_struct_get(rpc_server_functions,function_name,function) == 0){
        assert(rpc_struct_remove(rpc_server_functions,function_name) == 0);

        rpc_struct_free(function->function_context);
        free(function->function_name);
        free(function->prototype);
        free(function);
    }
}

struct rpc_updated_argument_info{
    char key[sizeof(int) * 2];
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

    keys = rpc_struct_getkeys(arguments);
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
    char** keys = rpc_struct_getkeys(arguments);
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
    char key[sizeof(int) * 2];
    if(!rpc_server_compare_protos(arguments,function->prototype,function->prototype_len)) return RPC_PROTOTYPE_DIFFERENT;

     //libffi NOT allow types smaller than int exist in variadic arguments....
     if(rpc_server_is_variadic(arguments,function->prototype_len) && rpc_server_is_variadic_allowed(arguments,function->prototype_len) == 0) return RPC_VARIADIC_NOT_ALLOWED;


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
    rpc_thread_context_set(function->function_context);

    int return_is = -1;
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
            return_is = arg_info->index;
            do_set = 1;
            goto loop_end;  //skip switch case
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

    loop_end:
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

    if(return_is != -1){
        uint32_t cast = return_is;
        assert(rpc_struct_set(output,"return_is",cast) == 0); //fuck you GCC for this warning, fuck you C compilers, fuck you all
    } else {
        if(function->return_type != RPC_none && function->return_type != RPC_unknown){ //we cannot serialise RPC_unknown since it is a raw pointer
            if(!(rpc_is_pointer(function->return_type) && (void*)ffi_return == NULL)){
                struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);
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
    }
    return RPC_OK;
}

//REMOVE ON NEXT PHASE(NETWORK)
rpc_struct_t test_wrap(char* name, rpc_struct_t arguments){
    rpc_function_t fn = NULL;
    assert(rpc_struct_get(rpc_server_functions,name,fn) == 0);

    rpc_struct_t out = rpc_struct_create();

    assert(rpc_server_call(fn,arguments,out) == 0);
    rpc_struct_free(out);
    return out;
}
