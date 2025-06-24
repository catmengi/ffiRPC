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




#include <ffirpc/hashmap/hashmap_base.h>
#include <ffirpc/ptracker.h>
#include <ffirpc/hashtable.h>

#include <ffirpc/hashmap/hashmap.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

static HASHMAP(void, void) ptracker = {0}; //should be changed to void* keyed HT to prevent convert to string!
static int PT_inited = 0;

struct _prec{
    void* ptr;

    atomic_int context_refcount;
    void* context;

    struct prec_callbacks cbs;
    atomic_int refcount;
};

size_t ptracker_hash_ptr(const void* key){
    return hashmap_hash_default(&key,sizeof(&key));
}
int ptracker_ptr_cmp(const void* k1, const void* k2){
    return k1 != k2;
}
static void ptracker_init(){
    if(PT_inited == 0){
        hashmap_init(&ptracker,ptracker_hash_ptr,ptracker_ptr_cmp);
        PT_inited = 1;
    }
}
prec_t prec_get(void* ptr){
    ptracker_init();
    return hashmap_get(&ptracker, ptr);
}

prec_t prec_new(void* ptr, struct prec_callbacks cbs){
    ptracker_init();
    assert(prec_get(ptr) == NULL);

    prec_t new = malloc(sizeof(*new)); assert(new);
    new->ptr = ptr;
    new->refcount = 0; //always increment after new!
    new->context = NULL;
    new->context_refcount = 1; //it will only be incremented on prec_merge
    new->cbs = cbs;

    assert(hashmap_insert(&ptracker,ptr,new,NULL) == 1);

    return new;
}

//force deletion
void prec_delete(prec_t prec){
    if(prec){
        hashmap_remove(&ptracker, prec->ptr);

        if(prec->cbs.zero) prec->cbs.zero(prec);
        free(prec);
    }
}

void prec_increment(prec_t prec, void* udata){
    if(prec){
        prec->refcount++;
        if(prec->cbs.increment) prec->cbs.increment(prec,udata);
    }
}
void prec_decrement(prec_t prec, void* udata){
    if(prec){
        if(prec->refcount > 0){
            prec->refcount--;
            if(prec->cbs.decrement) prec->cbs.decrement(prec,udata);
        }
        if(prec->refcount == 0) prec_delete(prec); //zeroed out, delete it
    }
}

int prec_refcount(prec_t prec){
    return (prec != NULL ? prec->refcount : 0);
}
void* prec_context_get(prec_t prec){
    return (prec != NULL ? prec->context : NULL);
}
void prec_context_set(prec_t prec, void* context){
    prec != NULL ? prec->context = context : assert(prec);
}
void* prec_ptr(prec_t prec){
    return (prec != NULL ? prec->ptr : NULL);
}
