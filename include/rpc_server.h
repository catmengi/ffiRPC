#include "rpc_thread_context.h"
#include "rpc_struct.h"

#include <ffi.h>

#define ARRAY_SIZE(x) ({sizeof((x)) / sizeof((x)[0]);})

int rpc_server_add_function(char* function_name, void* function_ptr,enum rpc_types return_type, enum rpc_types* prototype, int prototype_len);
void rpc_server_remove_function(char* function_name);
