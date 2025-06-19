#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/rpc_object.h"
#include "../include/sc_queue.h"

#include <assert.h>
#include <ffi.h>

static rpc_struct_t RO_cobject = NULL;
static __thread rpc_struct_t RO_lobjects = NULL; //lobjects should be loaded manualy!

static __thread int RO_inited = 0;
static __thread struct sc_queue_ptr RO_object_stack;

void rpc_object_init(){
    RO_cobject = rpc_struct_create();
}

static void rpc_object_thread_init(){
    if(RO_inited == 0){
        sc_queue_init(&RO_object_stack);
        RO_inited = 1;
    }
}

int rpc_cobject_add(char* cobj_name, rpc_struct_t cobj){
    return rpc_struct_set(RO_cobject, cobj_name, cobj);
}
rpc_struct_t rpc_cobject_get(char* cobj_name){
    rpc_struct_t ret = NULL;
    rpc_struct_get(RO_cobject, cobj_name, ret);
    return ret;
}

static void rpc_cobject_push(rpc_struct_t cobject){
    rpc_object_thread_init(); //it will only init queue if it was called from this function once!
    sc_queue_add_last(&RO_object_stack, cobject);
}

static rpc_struct_t rpc_cobject_peek(){
    rpc_object_thread_init();
    assert(sc_queue_size(&RO_object_stack) != 0);
    return sc_queue_size(&RO_object_stack) == 0 ? NULL : sc_queue_peek_first(&RO_object_stack);
}

static void rpc_cobject_pop(){
    rpc_object_thread_init();
    sc_queue_del_first(&RO_object_stack);
}

rpc_struct_t rpc_object_get_local(){
    rpc_object_thread_init();
    rpc_struct_t ret = NULL;
    if(RO_lobjects){
        rpc_struct_t current_cobject = rpc_cobject_peek();
        if(current_cobject){
            if(rpc_struct_get(RO_lobjects,rpc_struct_id_get(current_cobject),ret) != 0){
                ret = rpc_struct_create();
                assert(rpc_struct_set(RO_lobjects, rpc_struct_id_get(current_cobject), ret) == 0);
            }
        }
    }
    return ret;
}

int rpc_cobject_remove(char* cobj_name){
    rpc_object_thread_init();

    if(sc_queue_size(&RO_object_stack) != 0) return 1;
    return rpc_struct_remove(RO_cobject,cobj_name);
}

void rpc_object_load_locals(rpc_struct_t lobjects){
    RO_lobjects = lobjects;
}

//RPC object call ==============================================

static ffi_type* rpctype_to_libffi[RPC_unknown + 1] = {
    &ffi_type_void,   &ffi_type_uint64,
    &ffi_type_double, &ffi_type_pointer,
    &ffi_type_pointer,&ffi_type_pointer,
    &ffi_type_pointer,&ffi_type_pointer,
};

typedef struct{
    char key[sizeof(int) * 4];
    uint64_t hash;

    enum rpc_types type;
    void* raw_ptr;
    int index;
}rpc_updated_argument_info;

static int proto_equals(enum rpc_types* sproto, int sproto_len, rpc_struct_t cl_args){
    if(rpc_struct_length(cl_args) < sproto_len) return 0;

    char el[sizeof(int) * 4];
    for(int i = 0; i < sproto_len; i++){
        sprintf(el, "%d",i);
        if(rpc_struct_typeof(cl_args,el) != sproto[i]) return 0;
    }
    return 1;
}

int rpc_cobject_call(rpc_struct_t obj, char* fn_name, rpc_struct_t params, rpc_struct_t output){
    if(obj == NULL) return ERR_RPC_DOESNT_EXIST;

    rpc_function_t fn = NULL;
    if(rpc_struct_get(obj, fn_name, fn)) return ERR_RPC_DOESNT_EXIST;

    if(!proto_equals(rpc_function_get_prototype(fn),rpc_function_get_prototype_len(fn),params)) return ERR_RPC_PROTOTYPE_DIFFERENT;

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

    rpc_cobject_push(obj);
    ffi_call(&cif,rpc_function_get_fnptr(fn),&ffi_return,ffi_arguments);
    rpc_cobject_pop();

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
            if(rpc_is_pointer(element->type)){
                element->data = (void*)ffi_return;
            } else {
                element->length = sizeof(ffi_return);
                element->data = malloc(element->length); assert(element->data);
                memcpy(element->data,&ffi_return,element->length);
            }
            rpc_struct_set_internal(output,"return",element);
            if(rpc_function_get_return_type(fn) == RPC_string) free((char*)ffi_return); //can free, because in rpc_struct_set_internal in was strdup-ed
        }
    }
    return ERR_RPC_OK;
}

//==============================================================
