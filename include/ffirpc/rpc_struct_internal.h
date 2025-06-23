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

#pragma once

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <ffirpc/sc_queue.h>
#include <ffirpc/rpc_struct.h>

#define INTERNAL_API

struct rpc_container_element{
    void* data;
    size_t length;
    enum rpc_types type;
};

typedef void (*rpc_struct_free_cb)(void*);
typedef struct{
    rpc_struct_t origin;
    char* name;
    rpc_struct_free_cb free;
}prec_rpc_udata;

typedef struct{
    hashtable* keys;
    rpc_struct_free_cb free;
    pthread_mutex_t lock;
}rpc_struct_prec_ptr_ctx;

INTERNAL_API extern char ID_alphabet[37];
INTERNAL_API extern struct prec_callbacks rpc_struct_default_prec_cbs;
INTERNAL_API rpc_struct_free_cb rpc_freefn_of(enum rpc_types type);
INTERNAL_API void rpc_struct_free_internals(rpc_struct_t rpc_struct); //same as rpc_struct_free but doesnt call free on rpc_struct_t
INTERNAL_API size_t rpc_struct_memsize();
INTERNAL_API void rpc_struct_prec_ctx_destroy(prec_t prec, void (*destroyer)(void*, char*));

#define copy(input) ({void* __out = malloc(sizeof(*input)); assert(__out); memcpy(__out,input,sizeof(*input)); (__out);})
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

