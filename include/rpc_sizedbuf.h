#pragma  once

#include <stdlib.h>
#include <stdint.h>

typedef struct _rpc_sizedbuf *rpc_sizedbuf_t;

rpc_sizedbuf_t rpc_sizedbuf_create(char* buf, size_t length); //creates a new sizedbuf, "buf" will be copyed and returned via rpc_sizedbuf_getbuf in future. "length" is length of buf

char* rpc_sizedbuf_getbuf(rpc_sizedbuf_t szbuf, size_t* out_length); //return copyed "buf" from rpc_sizedbuf_create, in out_length will be placed its length

void rpc_sizedbuf_free(rpc_sizedbuf_t szbuf); //free sizedbuf and copyed "buf"

uint64_t rpc_sizedbuf_hash(rpc_sizedbuf_t szbuf); //return a hash of szbuf

char* rpc_sizedbuf_serialise(rpc_sizedbuf_t szbuf, size_t* out_length);
rpc_sizedbuf_t rpc_sizedbuf_unserialise(char* buf);
