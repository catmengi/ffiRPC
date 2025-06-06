#include "../include/rpc_function.h"
#include "../include/rpc_struct.h"

#include <assert.h>
#include <stdint.h>
#include <time.h>

#define NOT_IMPLEMENTED

struct _rpc_function{
    enum rpc_types* prototype;
    int prototype_len;

    enum rpc_types return_type;
    void* fnptr;
};

rpc_function_t rpc_function_create(){
    rpc_function_t fn = calloc(1,sizeof(*fn));
    return fn;
}

void rpc_function_free(rpc_function_t fn){
    if(fn){
        free(fn->prototype);
        free(fn);
    }
}

char* rpc_function_serialise(rpc_function_t fn, size_t* out_len){
    rpc_struct_t serialise = rpc_struct_create();

    if(fn->prototype){
        char conv[rpc_function_get_prototype_len(fn)];
        enum rpc_types* proto = rpc_function_get_prototype(fn);

        for(int i = 0; i < rpc_function_get_prototype_len(fn); i++){
            conv[i] = proto[i];
        }
        rpc_struct_set(serialise, "prototype", rpc_sizedbuf_create((char*)conv, rpc_function_get_prototype_len(fn)));
    }
    rpc_struct_set(serialise, "return_type", (uint8_t)fn->return_type);

    char* ret = rpc_struct_serialise(serialise,out_len);
    rpc_struct_free(serialise);

    return ret;
}
rpc_function_t rpc_function_unserialise(char* buf){
    rpc_struct_t unserialise = rpc_struct_unserialise(buf);
    rpc_function_t fn = rpc_function_create();

    rpc_sizedbuf_t serproto = NULL;
    if(rpc_struct_get(unserialise, "prototype", serproto) == 0){
        size_t serproto_l = 0;
        char* conv = rpc_sizedbuf_getbuf(serproto,&serproto_l);

        enum rpc_types proto[serproto_l];
        for(int i = 0; i < serproto_l; i++){
            proto[i] = conv[i];
        }
        rpc_function_set_prototype(fn,proto,serproto_l);
    }
    uint8_t conv_rettype = 0;
    assert(rpc_struct_get(unserialise,"return_type", conv_rettype) == 0);

    rpc_function_set_return_type(fn,conv_rettype);

    rpc_struct_free(unserialise);
    return fn;
}

void* rpc_function_get_fnptr(rpc_function_t fn){
    return fn != NULL ? fn->fnptr : NULL;
}
void rpc_function_set_fnptr(rpc_function_t fn, void* fnptr){
    if(fn){
        fn->fnptr = fnptr;
    }
}

void rpc_function_set_prototype(rpc_function_t fn, enum rpc_types* prototype, int prototype_len){
    if(fn){
        if(fn->prototype)
            free(fn->prototype);

        fn->prototype = malloc(prototype_len * sizeof(*prototype)); assert(fn->prototype);
        fn->prototype_len = prototype_len;

        memcpy(fn->prototype, prototype, sizeof(*fn->prototype) * fn->prototype_len);
    }
}

enum rpc_types* rpc_function_get_prototype(rpc_function_t fn){
    return fn->prototype;
}
int rpc_function_get_prototype_len(rpc_function_t fn){
    return fn->prototype_len;
}
void rpc_function_set_return_type(rpc_function_t fn, enum rpc_types return_type){
    if(fn){
        fn->return_type = return_type;
    }
}

enum rpc_types rpc_function_get_return_type(rpc_function_t fn){
    return fn->return_type;
}

