#include "../include/ptracker.h"
#include "../include/hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

static hashtable* ptracker = NULL; //should be changed to void* keyed HT to prevent convert to string!

struct _prec{
    void* ptr;
    void* context;

    struct prec_callbacks cbs;
    atomic_int refcount;
};

prec_t prec_get(void* ptr){
    if(ptracker == NULL){ //not inited yet
        ptracker = hashtable_create();
        return NULL; //because it is newly inited and does not have any data
    }
    char acc[sizeof(void*) * 4];
    sprintf(acc,"%p",ptr);

    return hashtable_get(ptracker,acc);
}

prec_t prec_new(void* ptr, struct prec_callbacks cbs){
    ptracker == NULL ? ptracker = hashtable_create() : assert(prec_get(ptr) == NULL); //check that element wasnt set before should only be when ptracker was already inited

    prec_t new = malloc(sizeof(*new)); assert(new);
    new->ptr = ptr;
    new->refcount = 0; //always increment after new!
    new->context = NULL;
    new->cbs = cbs;

    char acc[sizeof(void*) * 4];
    sprintf(acc,"%p",ptr);
    hashtable_set(ptracker,strdup(acc),new);

    return new;
}

//force deletion
void prec_delete(prec_t prec){
    if(prec){
        char acc[sizeof(void*) * 4];
        sprintf(acc,"%p",prec->ptr);
        char* kfree = ptracker->body[hashtable_find_slot(ptracker,acc)].key;
        hashtable_remove(ptracker,acc);
        free(kfree);

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

prec_t* prec_get_all(size_t* size_out){
    assert(size_out);
    if(ptracker){
        *size_out = ptracker->size;
        return (prec_t*)hashtable_get_values(ptracker);
    }else {*size_out = 0; return NULL;}
}
