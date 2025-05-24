#pragma once
#include <sys/types.h>

typedef struct _prec* prec_t;

typedef void (*prec_callback)(prec_t prec, void* udata);

struct prec_callbacks{
    prec_callback increment, decrement, zero; //zero's udara is always NULL!
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
prec_t* prec_get_all(size_t* size_out);
