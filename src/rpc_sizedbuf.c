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

#include <assert.h>
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

char* rpc_sizedbuf_getbuf(rpc_sizedbuf_t szbuf, size_t* out_length){
    assert(szbuf); assert(out_length);

    *out_length = szbuf->length;
    return szbuf->buf;
}

void rpc_sizedbuf_free(rpc_sizedbuf_t szbuf){
    if(szbuf){
        free(szbuf->buf);
        free(szbuf);
    }
}

char* rpc_sizedbuf_serialise(rpc_sizedbuf_t szbuf, size_t* out_length){
    *out_length = sizeof(uint64_t) + szbuf->length;
    char* buf = malloc(*out_length); assert(buf);

    uint64_t U64_len = szbuf->length;
    memcpy(buf,&U64_len,sizeof(U64_len));
    memcpy(buf + sizeof(uint64_t),szbuf->buf,szbuf->length);

    return buf;
}

rpc_sizedbuf_t rpc_sizedbuf_unserialise(char* buf){
    uint64_t U64_len = 0;
    memcpy(&U64_len,buf,sizeof(U64_len));

    return rpc_sizedbuf_create(buf + sizeof(uint64_t),(size_t)U64_len);
}

uint64_t rpc_sizedbuf_hash(rpc_sizedbuf_t szbuf){
    assert(szbuf);
    uint64_t hash = 0;

    murmur((uint8_t*)szbuf->buf,szbuf->length); hash += szbuf->length;
    murmur((uint8_t*)&hash,sizeof(uint64_t));
    return hash;
}


