#include "../include/rpc_thread_context.h"
#include "../include/hashtable.h"

#include <assert.h>
#include <pthread.h>

static hashtable* thread_context = NULL;

__attribute__((constructor))
void rpc_init_thread_context(){
    thread_context = hashtable_create();
    assert(thread_context);
}

void rpc_thread_context_set(rpc_struct_t rpc_struct){
    assert(thread_context);

    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];
    sprintf(ht_access,"%p",thread_as_ptr);

    if(rpc_struct){
        hashtable_set(thread_context,strdup(ht_access),rpc_struct);
    } else {char* free_key = thread_context->body[hashtable_find_slot(thread_context,ht_access)].key; hashtable_remove(thread_context,ht_access); free(free_key);}
}
rpc_struct_t rpc_thread_context_get(){
    assert(thread_context);
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);

    return hashtable_get(thread_context,ht_access);
}
