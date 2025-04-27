#include "ffiRPC_thread_context.h"
#include "hashtable.c/hashtable.h"

#include <assert.h>
#include <pthread.h>


hashtable* thread_context = NULL;

void ffiRPC_init_thread_context(){
    if(thread_context == NULL){
        thread_context = hashtable_create();
        assert(thread_context);
    }
}

int ffiRPC_thread_context_get_key(pthread_key_t* output){
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);
    pthread_key_t* key = hashtable_get(thread_context,ht_access);
    if(key == NULL) return 1;

    *output = *key;
    return 0;
}
void ffiRPC_thread_context_set_key(){
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);
    pthread_key_t* set_key = malloc(sizeof(*set_key)); assert(set_key);
    assert(pthread_key_create(set_key,NULL) == 0);

    hashtable_set(thread_context,strdup(ht_access),set_key);
}
void ffiRPC_thread_context_remove_key(){
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);
    if(hashtable_get(thread_context,ht_access) != NULL){
        unsigned int slot = hashtable_find_slot(thread_context,ht_access);
        char* free_key = thread_context->body[slot].key;

        pthread_key_delete(*(pthread_key_t*)thread_context->body[slot].value);
        free(thread_context->body[slot].value);

        hashtable_remove(thread_context,ht_access);

        free(free_key);
    }
}

void ffiRPC_thread_context_set(ffiRPC_struct_t ffiRPC_struct){
    pthread_key_t key = 0;
    if(ffiRPC_thread_context_get_key(&key) != 0){
        ffiRPC_thread_context_set_key();

        assert(ffiRPC_thread_context_get_key(&key) == 0);
    }
    pthread_setspecific(key,ffiRPC_struct);
}
ffiRPC_struct_t ffiRPC_thread_context_get(){
    pthread_key_t key = 0;
    return (ffiRPC_struct_t)(ffiRPC_thread_context_get_key(&key) != 0 ? NULL : pthread_getspecific(key));
}

int main(){
    ffiRPC_init_thread_context();
    ffiRPC_thread_context_set(ffiRPC_struct_create());
    assert(ffiRPC_thread_context_get() != NULL);
}
