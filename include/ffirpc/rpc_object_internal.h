#pragma once
#include <ffirpc/rpc_struct.h>

#include <ffi.h>

void rpc_lobjects_load(rpc_struct_t lobjects);
extern ffi_type* rpctype_to_libffi[];
