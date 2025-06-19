#include "rpc_struct.h"

#ifdef RPC_INIT
void rpc_object_init();
#endif

rpc_struct_t rpc_object_get_local();
void rpc_object_load_locals(rpc_struct_t lobjects);
int rpc_cobject_add(char* cobj_name, rpc_struct_t cobj);
int rpc_cobject_remove(char* cobj_name); //SHOULD NEVER BE CALLED FROM RPC_FUNCTION!
rpc_struct_t rpc_cobject_get(char* cobj_name);
int rpc_cobject_call(rpc_struct_t obj, char* fn_name, rpc_struct_t params, rpc_struct_t output);
