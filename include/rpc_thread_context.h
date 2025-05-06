#include <pthread.h>
#include <stdio.h>

#include "rpc_struct.h"

void rpc_thread_context_set(rpc_struct_t rpc_struct); //sets rpc_struct for thread
rpc_struct_t rpc_thread_context_get(); //return rpc_struct for this thread. RETURN: NULL if doesnt exist
