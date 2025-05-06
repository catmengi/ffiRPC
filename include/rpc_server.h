#include "rpc_thread_context.h"
#include "rpc_struct.h"

#include <ffi.h>

#define ARRAY_SIZE(x) ({sizeof((x)) / sizeof((x)[0]);})

typedef struct _rpc_server* rpc_server_t;

rpc_server_t rpc_server_create();
int rpc_server_add_function(rpc_server_t server, char* function_name, void* function_ptr,enum rpc_types return_type, enum rpc_types* prototype, int prototype_len);
void rpc_server_remove_function(rpc_server_t server, char* function_name);
void rpc_server_free(rpc_server_t server);
