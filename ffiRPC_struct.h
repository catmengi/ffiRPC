#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

#include "hashtable.c/hashtable.h"

enum ffiRPC_types{
    ffiRPC_char = 1,
    ffiRPC_int8 = 1,
    ffiRPC_uint8,
    ffiRPC_int16,
    ffiRPC_uint16,
    ffiRPC_int32,
    ffiRPC_uint32,
    ffiRPC_int64,
    ffiRPC_uint64,
    ffiRPC_double,

    ffiRPC_string,
    ffiRPC_struct,

    ffiRPC_unknown,
};

struct ffiRPC_container_element{
    void* data;
    size_t length;
    enum ffiRPC_types type;
};
struct ffiRPC_struct{
    hashtable* ht;
    atomic_size_t size;
};


#define CType_to_ffiRPC(Native_type) _Generic(Native_type,                     \
                                    char                 : ffiRPC_char,        \
                                    int8_t               : ffiRPC_int8,        \
                                    uint8_t              : ffiRPC_uint8,       \
                                    int16_t              : ffiRPC_int16,       \
                                    uint16_t             : ffiRPC_uint16,      \
                                    int32_t              : ffiRPC_int32,       \
                                    uint32_t             : ffiRPC_uint32,      \
                                    int64_t              : ffiRPC_int64,       \
                                    uint64_t             : ffiRPC_uint64,      \
                                    float                : ffiRPC_double,      \
                                    double               : ffiRPC_double,      \
                                    char*                : ffiRPC_string,      \
                                    ffiRPC_struct_t      : ffiRPC_struct,      \
                                    default              : ffiRPC_unknown      \
)


static int ffiRPC_is_pointer(enum ffiRPC_types type){
    int ret = 0;
    if(type == ffiRPC_struct || type == ffiRPC_string || type == ffiRPC_unknown) ret = 1;

    return ret;
}


typedef struct ffiRPC_struct *ffiRPC_struct_t;


static inline ffiRPC_struct_t ffiRPC_struct_create(){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->size = 0;

    return ffiRPC_struct;
}


#define C_to_ffiRPC(element,var)\
    element->type = CType_to_ffiRPC(var);\
    if(ffiRPC_is_pointer(element->type)){\
        void* ptr = (void*)var;\
        if(element->type == ffiRPC_string){\
            element->data = strdup(ptr);\
            assert(element->data);\
        } else element->data = ptr;\
            element->length = 0;\
    } else {\
        typeof(var) cpy_var = var;\
        element->data = malloc(sizeof(cpy_var));\
        assert(element->data);\
        element->length = sizeof(cpy_var);\
        memcpy(element->data,(void*)&cpy_var,element->length);\
    }


#define ffiRPC_struct_set(ffiRPC_struct, key, type)\
    struct ffiRPC_container_element* element = malloc(sizeof(*element)); assert(element);\
    C_to_ffiRPC(element,type);\
    hashtable_set(ffiRPC_struct->ht,strdup(key),element);\
    ffiRPC_struct->size++;
