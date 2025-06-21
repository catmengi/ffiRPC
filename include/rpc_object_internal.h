#pragma once
#include "rpc_struct.h"

#include <ffi.h>

extern rpc_struct_t RO_cobject_bname;
extern rpc_struct_t RO_cobject_bid;

void rpc_lobjects_load(rpc_struct_t lobjects);
extern ffi_type* rpctype_to_libffi[];
