#pragma once
#include <sys/types.h>

typedef struct _prec* prec_t;

struct prec_callbacks{
    void (*increment)(prec_t prec, void* udata);
    void (*decrement)(prec_t prec, void* udata);
    void (*zero)(prec_t prec);
};

prec_t prec_get(void* ptr);
prec_t prec_new(void* ptr, struct prec_callbacks cbs);
void prec_delete(prec_t prec);
void prec_increment(prec_t prec, void* udata);
void prec_decrement(prec_t prec, void* udata);
int prec_refcount(prec_t prec);
void* prec_context_get(prec_t prec);
void prec_context_set(prec_t prec, void* context);
void* prec_ptr(prec_t prec);
