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


