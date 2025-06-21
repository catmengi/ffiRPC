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




#include "../include/rpc_function.h"
#include "../include/rpc_struct.h"

#include <assert.h>
#include <jansson.h>
#include <stdint.h>
#include <time.h>

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

#define STRINGIFY(x) #x
json_t* rpc_function_serialize(rpc_function_t fn){
    json_t* root = json_object(); assert(root);

    json_object_set_new(root,"type",json_string(STRINGIFY(RPC_function)));
    json_object_set_new(root,"return_type",json_integer(fn->return_type));

    if(fn->prototype && fn->prototype_len > 0){
        json_t* array = json_array();
        json_object_set_new(root,"prototype",array);

        for(int i = 0; i < fn->prototype_len; i++){
            json_array_set_new(array,i,json_integer(fn->prototype[i]));
        }
    }

    return root;
}
rpc_function_t rpc_function_deserialize(json_t* json){
    rpc_function_t fn = rpc_function_create();
    rpc_function_set_fnptr(fn,NULL);

    json_t* type = json_object_get(json,"type");
    if(type){
        const char* stype = json_string_value(type);
        if(stype == NULL || strcmp(stype, STRINGIFY(RPC_function)) != 0) goto bad_exit;
    } else goto bad_exit;

    fn->return_type = json_integer_value(json_object_get(json,"return_type"));

    json_t* array = json_object_get(json,"prototype");
    if(array){
        fn->prototype_len = json_array_size(array);
        fn->prototype = malloc(sizeof(*fn->prototype) * fn->prototype_len); assert(fn->prototype);

        int i = 0;
        json_t* proto_el = NULL;
        json_array_foreach(array,i,proto_el){
            fn->prototype[i] = json_integer_value(proto_el);
        }
    }

    return fn;
bad_exit:
    rpc_function_free(fn);
    return NULL;
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

