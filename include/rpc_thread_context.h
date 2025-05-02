#include <pthread.h>
#include <stdio.h>

#include "rpc_struct.h"

void rpc_init_thread_context(); //init per-thread rpc_struct holder
void rpc_destroy_thread_context(); //destroy per-thread rpc_struct holder
int rpc_is_thread_context_inited(); //return 1 if context inited. 0 if not

void rpc_thread_context_set(rpc_struct_t rpc_struct); //sets rpc_struct for thread
rpc_struct_t rpc_thread_context_get(); //return rpc_struct for this thread. RETURN: NULL if doesnt exist
