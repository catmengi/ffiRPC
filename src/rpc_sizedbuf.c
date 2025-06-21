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



#include "../include/rpc_sizedbuf.h"
#include "../include/hashtable.h"
#include "../include/ptracker.h"

#include <assert.h>
#include <jansson.h>
#include <string.h>
#include <stdint.h>

struct _rpc_sizedbuf{
    char* buf;
    size_t length;
};

rpc_sizedbuf_t rpc_sizedbuf_create(char* buf, size_t length){
    rpc_sizedbuf_t szbuf = malloc(sizeof(*szbuf)); assert(szbuf);

    szbuf->length = length;
    szbuf->buf = malloc(szbuf->length); assert(szbuf->buf);
    memcpy(szbuf->buf,buf,szbuf->length);

    return szbuf;
}

rpc_sizedbuf_t rpc_sizedbuf_copy(rpc_sizedbuf_t szbuf){
    size_t len = 0;
    return rpc_sizedbuf_create(rpc_sizedbuf_getbuf(szbuf,&len),len);
}

INTERNAL_API size_t rpc_sizedbuf_memsize(){
    return sizeof(struct _rpc_sizedbuf);
}

char* rpc_sizedbuf_getbuf(rpc_sizedbuf_t szbuf, size_t* out_length){
    assert(szbuf); assert(out_length);

    *out_length = szbuf->length;
    return szbuf->buf;
}
void rpc_sizedbuf_free_internals(rpc_sizedbuf_t szbuf){
    if(szbuf){
        free(szbuf->buf);
    }
}
void rpc_sizedbuf_free(rpc_sizedbuf_t szbuf){
    if(szbuf){
        prec_t prec = prec_get(szbuf);
        if(prec) {prec_delete(prec);}
        else{
            rpc_sizedbuf_free_internals(szbuf);
            free(szbuf);
        }
    }
}

#define STRINGIFY(x) #x
json_t* rpc_sizedbuf_serialize(rpc_sizedbuf_t szbuf){
    json_t* root = json_object(); assert(root);

    json_object_set_new(root,"type",json_string(STRINGIFY(RPC_sizedbuf)));

    json_t* array = json_array();
    json_object_set_new(root, "serialised", array);

    for(size_t i = 0; i < szbuf->length; i++){
        assert(json_array_append_new(array,json_integer(szbuf->buf[i])) == 0);
    }

    return root;
}

rpc_sizedbuf_t rpc_sizedbuf_deserialize(json_t* json){
    rpc_sizedbuf_t szbuf = malloc(sizeof(*szbuf)); assert(szbuf);
    json_t* type = json_object_get(json,"type");

    if(type){
        const char* stype = json_string_value(type);
        if(stype == NULL || strcmp(stype, STRINGIFY(RPC_sizedbuf)) != 0) goto bad_exit;
    } else goto bad_exit;

    json_t* array = json_object_get(json,"serialised");
    size_t i = 0;
    json_t* cur_num = NULL;

    szbuf->length = json_array_size(array);
    szbuf->buf = malloc(szbuf->length); assert(szbuf->buf);

    json_array_foreach(array,i,cur_num){
        szbuf->buf[i] = json_integer_value(json_array_get(array,i));
    }

    return szbuf;
bad_exit:
    rpc_sizedbuf_free(szbuf);
    return NULL;
}

uint64_t rpc_sizedbuf_hash(rpc_sizedbuf_t szbuf){
    assert(szbuf);
    uint64_t hash = 0;

    murmur((uint8_t*)szbuf->buf,szbuf->length); hash += szbuf->length;
    murmur((uint8_t*)&hash,sizeof(uint64_t));
    return hash;
}


