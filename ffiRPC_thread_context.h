#include <pthread.h>
#include <stdio.h>

#include "ffiRPC_struct.h"

void ffiRPC_init_thread_context(); //init per-thread ffiRPC_struct selector

void ffiRPC_thread_context_set(ffiRPC_struct_t ffiRPC_struct); //sets ffiRPC_struct for thread
ffiRPC_struct_t ffiRPC_thread_context_get(); //return ffiRPC_struct for this thread. RETURN: NULL if doesnt exist
