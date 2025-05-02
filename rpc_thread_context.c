#include "rpc_thread_context.h"
#include "hashtable.c/hashtable.h"

#include <assert.h>
#include <pthread.h>


static hashtable* thread_context = NULL;

void rpc_init_thread_context(){
    if(thread_context == NULL){
        thread_context = hashtable_create();
        assert(thread_context);
    }
}
void rpc_destroy_thread_context(){
    if(thread_context != NULL){
        for(size_t i = 0; i < thread_context->capacity; i++){
            if(thread_context->body[i].key != NULL && thread_context->body[i].key != (void*)0xDEAD){
                free(thread_context->body[i].key);
            }
        }

        hashtable_destroy(thread_context);
        thread_context = NULL;
    }
}
int rpc_is_thread_context_inited(){
    return thread_context != NULL;
}

void rpc_thread_context_set(rpc_struct_t rpc_struct){
    assert(thread_context);
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);
    hashtable_set(thread_context,strdup(ht_access),rpc_struct);
}
rpc_struct_t rpc_thread_context_get(){
    assert(thread_context);
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);

    return hashtable_get(thread_context,ht_access);
}
