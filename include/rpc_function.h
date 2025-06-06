#pragma once
#include "rpc_struct.h"

typedef struct _rpc_function* rpc_function_t;

rpc_function_t rpc_function_create();
void rpc_function_free(rpc_function_t fn);
char* rpc_function_serialise(rpc_function_t fn, size_t* out_len);
rpc_function_t rpc_function_unserialise(char* buf);
void* rpc_function_get_fnptr(rpc_function_t fn);
void rpc_function_set_fnptr(rpc_function_t fn, void* fnptr);
void rpc_function_set_prototype(rpc_function_t fn, enum rpc_types* prototype, int prototype_len);
enum rpc_types* rpc_function_get_prototype(rpc_function_t fn);
int rpc_function_get_prototype_len(rpc_function_t fn);
enum rpc_types rpc_function_get_return_type(rpc_function_t fn);
void rpc_function_set_return_type(rpc_function_t fn, enum rpc_types return_type);
