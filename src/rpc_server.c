#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/sc_queue.h"
#include "../include/rpc_thread_context.h"

#include <assert.h>
#include <ffi.h>

typedef struct{
    char key[sizeof(int) * 4];
    uint64_t hash;

    enum rpc_types type;
    void* raw_ptr;
    int index;
}rpc_updated_argument_info;

ffi_type* rpctype_to_libffi[RPC_duplicate] = {   //convert table used to convert from rpc_types to ffi type
    &ffi_type_void,
    &ffi_type_schar,
    &ffi_type_uint8, &ffi_type_sint16,
    &ffi_type_uint16, &ffi_type_sint32,
    &ffi_type_uint32, &ffi_type_sint64,
    &ffi_type_uint64, &ffi_type_double,

    &ffi_type_pointer, &ffi_type_pointer,
    &ffi_type_pointer, &ffi_type_pointer,
    &ffi_type_pointer
};

//======= Server global variables declaration ! ===========
static rpc_struct_t RS_objects = NULL;
static rpc_struct_t RS_id_object_map = NULL;
//=========================================================

void rpc_server_init(){
    RS_objects = rpc_struct_create();
    RS_id_object_map = rpc_struct_create();
}

rpc_struct_t rpc_server_create_object(const char* name){
    assert(name);

    rpc_struct_t obj = rpc_struct_create();
    if(rpc_struct_set(RS_objects,(char*)name,obj) != 0){
        rpc_struct_free(obj);
        return NULL;
    }
    rpc_struct_set(RS_id_object_map, rpc_struct_id_get(obj), obj);
    return obj;
}
int rpc_server_add_object(rpc_struct_t obj, const char* name){
    int ret = rpc_struct_set(RS_objects, (char*)name, obj);
    if(ret == 0){
        rpc_struct_set(RS_id_object_map, rpc_struct_id_get(obj), obj);
    }
    return ret;
}
int rpc_server_remove_object(const char* name){
    rpc_struct_t obj = NULL;
    int ret = 1;

    if((ret = rpc_struct_get(RS_objects, (char*)name, obj)) == 0){
        ret += rpc_struct_remove(RS_id_object_map, rpc_struct_id_get(obj));
        ret += rpc_struct_remove(RS_objects, (char*)name);
    }
    return ret;
}

inline static void rpc_server_ctx_load_object(rpc_struct_t obj){
    if(rpc_thread_context_get() == NULL) rpc_thread_context_set(rpc_struct_create());
    struct sc_queue_ptr* object_stack = NULL;
    if(rpc_struct_get(rpc_thread_context_get(), "object_stack", object_stack) != 0){
        object_stack = malloc(sizeof(*object_stack)); assert(object_stack);
        sc_queue_init(object_stack);
        assert(rpc_struct_set(rpc_thread_context_get(), "object_stack", object_stack) == 0);
    }
    rpc_struct_increment_refcount(obj); //we dont need it to free while it is still referenced somewhere outside other rpc_struct_t
    sc_queue_add_first(object_stack, obj);
}

inline static void rpc_server_ctx_unload_object(){
    struct sc_queue_ptr* object_stack = NULL;
    if(rpc_struct_get(rpc_thread_context_get(), "object_stack", object_stack) == 0){
        rpc_struct_t obj = sc_queue_del_first(object_stack);
        rpc_struct_decrement_refcount(obj);
    }
}

rpc_struct_t rpc_server_get_object(const char* name){
    struct sc_queue_ptr* object_stack = NULL;
    rpc_struct_t out = NULL;

    if(name == NULL){
        if(rpc_struct_get(rpc_thread_context_get(), "object_stack", object_stack) == 0) out = sc_queue_peek_first(object_stack);
    } else rpc_struct_get(RS_objects, (char*)name, out);

    return out;
}

int rpc_server_object_add_function(rpc_struct_t obj, const char* fn_name, rpc_function_t fn){
    assert(obj);
    assert(fn);

    if(rpc_function_get_fnptr(fn) == NULL) return 1; //You tried to add function without pointer or listed from client?

    return rpc_struct_set(obj,(char*)fn_name, fn);
}

int rpc_server_object_remove_function(rpc_struct_t obj, const char* fn_name){
    return rpc_struct_remove(obj,(char*)fn_name);
}

rpc_function_t rpc_server_object_get_function(rpc_struct_t obj, const char* fn_name){
    rpc_function_t ret = NULL;
    rpc_struct_get(obj,(char*)fn_name,ret);
    return ret;
}

static int proto_equals(enum rpc_types* sproto, int sproto_len, rpc_struct_t cl_args){
    if(rpc_struct_length(cl_args) < sproto_len) return 0;

    char el[sizeof(int) * 4];
    for(int i = 0; i < sproto_len; i++){
        sprintf(el, "%d",i);
        if(rpc_struct_typeof(cl_args,el) != sproto[i]) return 0;
    }
    return 1;
}

static int is_variadic_allowed(rpc_struct_t cl_args, int sproto_len){
    char el[sizeof(int) * 4];
    for(int i = sproto_len; i < rpc_struct_length(cl_args); i++){
        sprintf(el,"%d",i);
        if(rpc_struct_typeof(cl_args,el) < RPC_int32) return 0;
    }
    return 1;
}

//loads object into context and calls function!
static int rpc_server_call_object(rpc_struct_t obj, char* fn_name, rpc_struct_t params, rpc_struct_t output){
    rpc_function_t fn = NULL;
    if(rpc_struct_get(obj, fn_name, fn)) return ERR_RPC_DOESNT_EXIST;

    if(!proto_equals(rpc_function_get_prototype(fn),rpc_function_get_prototype_len(fn),params)) return ERR_RPC_PROTOTYPE_DIFFERENT;
    if(!is_variadic_allowed(params,rpc_function_get_prototype_len(fn))) return ERR_RPC_VARIADIC_NOT_ALLOWED;

    ffi_cif cif;
    ffi_type** ffi_prototype = malloc(sizeof(*ffi_prototype) * rpc_struct_length(params)); assert(ffi_prototype);

    char el[sizeof(int) * 4];
    for(int i = 0; i < rpc_struct_length(params); i++){
        sprintf(el,"%d",i);
        ffi_prototype[i] = rpctype_to_libffi[rpc_struct_typeof(params,el)];
    }
    assert(ffi_prep_cif_var(&cif,FFI_DEFAULT_ABI,rpc_function_get_prototype_len(fn),(int)rpc_struct_length(params),rpctype_to_libffi[rpc_function_get_return_type(fn)],ffi_prototype) == 0);
    void** ffi_arguments = calloc(rpc_struct_length(params),sizeof(void*)); assert(ffi_arguments);

    struct sc_queue_ptr updated_arguments;
    sc_queue_init(&updated_arguments);

    for(int i = 0; i < (int)rpc_struct_length(params); i++){
        sprintf(el,"%d",i);
        ffi_arguments[i] = calloc(1,sizeof(uint64_t)); assert(ffi_arguments[i]);
        rpc_struct_get_unsafe(params,el,*(uint64_t*)ffi_arguments[i]); //using unsafe, because type safe variant will crash us out

        enum rpc_types type = rpc_struct_typeof(params,el);

        if(rpc_is_pointer(type) && type != RPC_unknown){
            rpc_updated_argument_info* arg_info = calloc(1,sizeof(*arg_info)); assert(arg_info);

            arg_info->index = i;
            arg_info->type = type;
            arg_info->raw_ptr = *(void**)ffi_arguments[i];
            strcpy(arg_info->key,el);

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

    void* return_is = NULL;
    uint64_t ffi_return = 0;

    rpc_server_ctx_load_object(obj);
    ffi_call(&cif,rpc_function_get_fnptr(fn),&ffi_return,ffi_arguments);
    rpc_server_ctx_unload_object();

    free(ffi_prototype);

    for(int i = 0; i < (int)rpc_struct_length(params); i++)
        free(ffi_arguments[i]);
    free(ffi_arguments);

    int updated_arguments_len = sc_queue_size(&updated_arguments);
    for(int i = 0; i < updated_arguments_len; i++){
        rpc_updated_argument_info* arg_info = sc_queue_del_first(&updated_arguments);
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
            default:
                do_set = 1;  //looks like unhasheble type like RPC_function
                break;
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

    if(rpc_function_get_return_type(fn) != RPC_none && rpc_function_get_return_type(fn) != RPC_unknown){ //we cannot serialise RPC_unknown since it is a raw pointer
        if(!(rpc_is_pointer(rpc_function_get_return_type(fn)) && (void*)ffi_return == NULL)){
            struct rpc_container_element* element = calloc(1,sizeof(*element)); assert(element);
            element->type = rpc_function_get_return_type(fn);
            element->length = rpctype_sizes[element->type];
            if(rpc_is_pointer(element->type)){
                element->data = (void*)ffi_return;
            } else {
                element->data = malloc(element->length); assert(element->data);
                memcpy(element->data,&ffi_return,element->length);
            }
            rpc_struct_set_internal(output,"return",element);
            if(rpc_function_get_return_type(fn) == RPC_string) free((char*)ffi_return); //can free, because in rpc_struct_set_internal in was strdup-ed
        }
    }
    return ERR_RPC_OK;
}

