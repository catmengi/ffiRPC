#pragma once

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "rpc_struct.h"

struct rpc_container_element{
    void* data;
    size_t length;
    enum rpc_types type;
};

#include "rpc_struct.h"

#define rpc_cast_value(output, input) typeof(output) cpy = (typeof(output))input; output = cpy;

#define c_to_rpc(element,var)({\
    element->type = ctype_to_rpc(typeof(var));\
    char __tmp[sizeof(typeof(var))]; *(typeof(var)*)__tmp = var;\
    if(rpc_is_pointer(element->type)){\
        element->length = 0;\
        element->data = *(void**)__tmp;\
    } else {\
        element->data = malloc(sizeof(typeof(var)));\
        assert(element->data);\
        element->length = sizeof(typeof(var));\
        memcpy(element->data,__tmp,element->length);\
    }})

