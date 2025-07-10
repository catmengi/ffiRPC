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

#include <ffirpc/rpc_struct.h>
#include <ffirpc/rpc_config.h>

#define INTERNAL_API
typedef struct _rpc_function* rpc_function_t;

rpc_function_t rpc_function_create();
void rpc_function_free(rpc_function_t fn);

#ifdef RPC_SERIALISERS
json_t* rpc_function_serialize(rpc_function_t fn);
rpc_function_t rpc_function_deserialize(json_t* json);
#endif

void* rpc_function_get_fnptr(rpc_function_t fn);
void rpc_function_set_fnptr(rpc_function_t fn, void* fnptr);
void rpc_function_set_prototype(rpc_function_t fn, enum rpc_types* prototype, int prototype_len);
enum rpc_types* rpc_function_get_prototype(rpc_function_t fn);
int rpc_function_get_prototype_len(rpc_function_t fn);
enum rpc_types rpc_function_get_return_type(rpc_function_t fn);
void rpc_function_set_return_type(rpc_function_t fn, enum rpc_types return_type);
rpc_function_t rpc_function_copy(rpc_function_t fn);
INTERNAL_API void rpc_function_free_internals(rpc_function_t fn);
INTERNAL_API size_t rpc_function_memsize();
