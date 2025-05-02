#pragma  once

#include <stdlib.h>

typedef struct _ffiRPC_sizedbuf *ffiRPC_sizedbuf_t;

ffiRPC_sizedbuf_t ffiRPC_sizedbuf_create(char* buf, size_t length); //creates a new sizedbuf, "buf" will be copyed and returned via ffiRPC_sizedbuf_getbuf in future. "length" is length of buf

char* ffiRPC_sizedbuf_getbuf(ffiRPC_sizedbuf_t szbuf, size_t* out_length); //return copyed "buf" from ffiRPC_sizedbuf_create, in out_length will be placed its length

void ffiRPC_sizedbuf_free(ffiRPC_sizedbuf_t szbuf); //free sizedbuf and copyed "buf"

char* ffiRPC_sizedbuf_serialise(ffiRPC_sizedbuf_t szbuf, size_t* out_length);
ffiRPC_sizedbuf_t ffiRPC_sizedbuf_unserialise(char* buf);
