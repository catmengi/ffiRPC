#include "ffiRPC_sizedbuf.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>

struct _ffiRPC_sizedbuf{
    char* buf;
    size_t length;
};

ffiRPC_sizedbuf_t ffiRPC_sizedbuf_create(char* buf, size_t length){
    ffiRPC_sizedbuf_t szbuf = malloc(sizeof(*szbuf)); assert(szbuf);

    szbuf->length = length;
    szbuf->buf = malloc(szbuf->length); assert(szbuf->buf);
    memcpy(szbuf->buf,buf,szbuf->length);

    return szbuf;
}

char* ffiRPC_sizedbuf_getbuf(ffiRPC_sizedbuf_t szbuf, size_t* out_length){
    assert(szbuf); assert(out_length);

    *out_length = szbuf->length;
    return szbuf->buf;
}

void ffiRPC_sizedbuf_free(ffiRPC_sizedbuf_t szbuf){
    if(szbuf){
        free(szbuf->buf);
        free(szbuf);
    }
}

char* ffiRPC_sizedbuf_serialise(ffiRPC_sizedbuf_t szbuf, size_t* out_length){
    *out_length = sizeof(uint64_t) + szbuf->length;
    char* buf = malloc(*out_length); assert(buf);

    uint64_t U64_len = szbuf->length;
    memcpy(buf,&U64_len,sizeof(U64_len));
    memcpy(buf + sizeof(uint64_t),szbuf->buf,szbuf->length);

    return buf;
}

ffiRPC_sizedbuf_t ffiRPC_sizedbuf_unserialise(char* buf){
    uint64_t U64_len = 0;
    memcpy(&U64_len,buf,sizeof(U64_len));

    return ffiRPC_sizedbuf_create(buf + sizeof(uint64_t),(size_t)U64_len);
}


