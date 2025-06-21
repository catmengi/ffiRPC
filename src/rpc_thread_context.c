// MIT License
//
// Copyright (c) 2025 Catmengi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



#include "../include/rpc_thread_context.h"
#include "../include/hashtable.h"

#include <assert.h>
#include <pthread.h>

static hashtable* thread_context = NULL;

void rpc_init_thread_context(){
    thread_context = hashtable_create();
    assert(thread_context);
}

void rpc_deinit_thread_context(){
    if(thread_context){
        for(size_t i = 0; i < thread_context->capacity; i++){
            if(thread_context->body[i].key != NULL && thread_context->body[i].key != (void*)0xDEAD){
                free(thread_context->body[i].key);
            }
        }
        hashtable_destroy(thread_context);
        thread_context = NULL;
    }
}

rpc_struct_t rpc_thread_context_set(rpc_struct_t rpc_struct){
    assert(thread_context);

    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];
    sprintf(ht_access,"%p",thread_as_ptr);

    if(rpc_struct){
        if(hashtable_get(thread_context,ht_access)){
            char* heap_key = thread_context->body[hashtable_find_slot(thread_context,ht_access)].key;
            hashtable_set(thread_context,heap_key,rpc_struct);
        } else hashtable_set(thread_context,strdup(ht_access),rpc_struct);
        return rpc_struct;
    } else {char* free_key = thread_context->body[hashtable_find_slot(thread_context,ht_access)].key; hashtable_remove(thread_context,ht_access); free(free_key); return NULL;}
}
rpc_struct_t rpc_thread_context_get(){
    assert(thread_context);
    void* thread_as_ptr = (void*)pthread_self();
    char ht_access[sizeof(void*) * 2];

    sprintf(ht_access,"%p",thread_as_ptr);

    return hashtable_get(thread_context,ht_access);
}
