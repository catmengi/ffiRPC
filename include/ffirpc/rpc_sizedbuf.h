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



#pragma  once

#define INTERNAL_API
#include <ffirpc/rpc_config.h>

#include <stdlib.h>
#include <stdint.h>

#ifdef RPC_SERIALISERS
#include <jansson.h>
#endif

typedef struct _rpc_sizedbuf *rpc_sizedbuf_t;

rpc_sizedbuf_t rpc_sizedbuf_create(char* buf, size_t length); //creates a new sizedbuf, "buf" will be copyed and returned via rpc_sizedbuf_getbuf in future. "length" is length of buf

char* rpc_sizedbuf_getbuf(rpc_sizedbuf_t szbuf, size_t* out_length); //return copyed "buf" from rpc_sizedbuf_create, in out_length will be placed its length

void rpc_sizedbuf_free(rpc_sizedbuf_t szbuf); //free sizedbuf and copyed "buf"
INTERNAL_API void rpc_sizedbuf_free_internals(rpc_sizedbuf_t szbuf);

uint64_t rpc_sizedbuf_hash(rpc_sizedbuf_t szbuf); //return a hash of szbuf

#ifdef RPC_SERIALISERS
json_t* rpc_sizedbuf_serialize(rpc_sizedbuf_t szbuf);
rpc_sizedbuf_t rpc_sizedbuf_deserialize(json_t* json);
#endif

rpc_sizedbuf_t rpc_sizedbuf_copy(rpc_sizedbuf_t szbuf);

INTERNAL_API size_t rpc_sizedbuf_memsize();
