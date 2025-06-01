#include "../include/rpc_function.h"
#include "../include/rpc_struct.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>

#define NOT_IMPLEMENTED

struct _rpc_function{
    NOT_IMPLEMENTED rpc_struct_t rpc_info;
                           //rpc_info is NULL when it is used by server to call "original" function. NON NULL when this function was received through RPC client, rpc_info will contain:
                           //ffi closures, CIFs, pointer to client. You will be able to call fn_ptr just like any other function pointer through cast, closure will handle rest

    rpc_sizedbuf_t prototype_buf;
    enum rpc_types return_type;
    void* fn_ptr;
};

static rpc_function_t rpc_function_create_internal(void* fnptr,enum rpc_types return_type, rpc_sizedbuf_t prototype_buf){
    rpc_function_t fn = malloc(sizeof(*fn)); assert(fn);
    fn->prototype_buf = prototype_buf;
    fn->return_type = return_type;
    fn->fn_ptr = fnptr;
    return fn;
}
rpc_function_t rpc_function_create(void* fn_ptr,enum rpc_types return_type,enum rpc_types* prototype, int prototype_len){
    return rpc_function_create_internal(fn_ptr,return_type,prototype != NULL ? rpc_sizedbuf_create((char*)prototype, sizeof(*prototype) * prototype_len) : NULL);
}

void rpc_function_free(rpc_function_t fn){
    assert(fn);

    rpc_sizedbuf_free(fn->prototype_buf);
    free(fn);
}

char* rpc_function_serialise(rpc_function_t fn, size_t* out_len){
    rpc_struct_t ser = rpc_struct_create();

    if(fn->prototype_buf){
        rpc_struct_set(ser,"prototype",fn->prototype_buf);
        rpc_struct_increment_refcount(fn->prototype_buf); //we dont want that fn->prototype_buf will be freed without our permission, right?
    }
    assert(rpc_struct_set(ser,"return_type", (uint8_t)fn->return_type) == 0);

    char* ret = rpc_struct_serialise(ser,out_len);
    rpc_struct_free(ser);

    return ret;
}

rpc_function_t rpc_function_unserialise(char* buf){
    rpc_struct_t unser = rpc_struct_unserialise(buf);
    rpc_sizedbuf_t prototype_buf = NULL;

    uint8_t return_type = RPC_none;
    rpc_function_t ret = NULL;
    if(rpc_struct_get(unser, "prototype",prototype_buf) == 0){
        rpc_struct_increment_refcount(prototype_buf); //transfer ownership to us, same as in rpc_function_serialise
    }
    assert(rpc_struct_get(unser, "return_type", return_type) == 0);

    // BUILD CALLABLE PROXY FUNCTION VIA LIBFFI!
    ret = rpc_function_create_internal(NULL,return_type,prototype_buf);
    rpc_struct_free(unser);
    return ret;

}

void* rpc_function_get_fnptr(rpc_function_t fn){
    assert(fn);
    return fn->fn_ptr;
}
enum rpc_types* rpc_function_get_prototype(rpc_function_t fn, int* out_length){
    if(fn->prototype_buf != NULL){
        size_t szbuf_olen = 0;
        enum rpc_types* ret = (enum rpc_types*)rpc_sizedbuf_getbuf(fn->prototype_buf, &szbuf_olen);

        *out_length = szbuf_olen / sizeof(*ret);

        return ret;
    } else {*out_length = 0; return NULL;}
}
enum rpc_types rpc_function_get_return_type(rpc_function_t fn){
    return fn->return_type;
}

