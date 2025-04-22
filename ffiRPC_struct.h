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
typedef struct ffiRPC_struct *ffiRPC_struct_t;

ffiRPC_struct_t ffiRPC_struct_create(void);

int ffiRPC_is_pointer(enum ffiRPC_types type);
void ffiRPC_container_free(struct ffiRPC_container_element* element);

#define CType_to_ffiRPC(Native_type) _Generic((Native_type),                   \
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

#define ffiRPC_cast_value(output, input) typeof(output) cpy = input; output = cpy;

#define C_to_ffiRPC(element,var)({\
    element->type = CType_to_ffiRPC(var);\
    if(ffiRPC_is_pointer(element->type)){\
        void* ptr = NULL;\
        void* cpy_varV = (void*)var;\
        if(element->type == ffiRPC_string) {ptr = strdup(cpy_varV); assert(ptr);}\
        else ptr = cpy_varV;\
        element->data = ptr;\
        element->length = 0;\
    } else {\
        typeof(var) cpy_var = var;\
        element->data = malloc(sizeof(cpy_var));\
        assert(element->data);\
        element->length = sizeof(cpy_var);\
        memcpy(element->data,(void*)&cpy_var,element->length);\
    }})


/*Sets a structure element at ffiRPC_struct with type of "input" and value of "input".
 *NOTE: If you are passing string literal you SHOULD cast it to char*
 *NOTE: strings(char*) are always copied when passing into structure
 *NOTE: you can pass void* or any other pointer into the struct but it WONT be serialised
 *NOTE: "key" are always copied when passing into structure
 *
 *EXAMPLE: ffiRPC_struct_set(ffiRPC_struct,"check_int",(uint64_t)12345678);
*/
#define ffiRPC_struct_set(ffiRPC_struct, key, input)({\
    struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct->ht,key);\
    if(element == NULL) {element = malloc(sizeof(*element)); assert(element); ffiRPC_struct->size++;}\
    else{ffiRPC_container_free(element);}\
    C_to_ffiRPC(element,input);\
    hashtable_set(ffiRPC_struct->ht,strdup(key),element);\
    (int)(0);})

/*Get element from ffiRPC_struct at key "key" and writes it's data into "output". If element "key" does NOT exist returns 1 else 0
 *NOTE: you SHOULD type only NAME of output variable into "output", NOT &output !!!
 *
 *EXAMPLE: uint64_t input = 12345678;
 *         ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
 *         uint64_t output;
 *         assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
 *         assert(output == input);
*/
#define ffiRPC_struct_get(ffiRPC_struct, key, output)({int ret = 1;struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct->ht,key);\
                                                    if(element != NULL){assert(element->type == CType_to_ffiRPC(output));if(ffiRPC_is_pointer(element->type)){ffiRPC_cast_value(output,(typeof(output))element->data);} else{ffiRPC_cast_value(output,*(typeof(output)*)element->data);} ret = 0;}(ret);})
