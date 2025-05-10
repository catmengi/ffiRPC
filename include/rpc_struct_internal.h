#pragma once

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "rpc_struct.h"

struct rpc_container_element{
    void* data;
    size_t length;
    enum rpc_types type;

    atomic_size_t refcount;
    atomic_size_t copy_count; //used to properly increment refcount of copy
};

#define rpc_cast_value(output, input) typeof(output) cpy = (typeof(output))input; output = cpy;

#define c_to_rpc(element,var)({\
    element->type = ctype_to_rpc(typeof(var));\
    if(rpc_is_pointer(element->type)){\
        element->length = 0;\
        element->data = (void*)var;\
    } else {\
        typeof(var) cpy_var = var;\
        element->data = malloc(sizeof(cpy_var));\
        assert(element->data);\
        element->length = sizeof(cpy_var);\
        memcpy(element->data,(void*)&cpy_var,element->length);\
    }})

extern size_t rpctype_sizes[RPC_duplicate];
