#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "hashtable.c/hashtable.h"

enum ffiRPC_types{
    EffiRPC_char = 1,
    EffiRPC_int8 = 1,
    EffiRPC_uint8,
    EffiRPC_int16,
    EffiRPC_uint16,
    EffiRPC_int32,
    EffiRPC_uint32,
    EffiRPC_int64,
    EffiRPC_uint64,
    EffiRPC_double,

    EffiRPC_string,
    EffiRPC_struct,

    EffiRPC_unknown,
};

struct ffiRPC_container_element{
    void* data;
    size_t length;
    enum ffiRPC_types type;
};
struct _ffiRPC_struct{
    hashtable* ht;
    hashtable* anti_double_free;

    atomic_size_t size;
};
typedef struct _ffiRPC_struct *ffiRPC_struct_t;

//==================== public API's ===================

ffiRPC_struct_t ffiRPC_struct_create(void);  //creates a new ffiRPC_struct_t

void ffiRPC_struct_free(ffiRPC_struct_t ffiRPC_struct);  //frees ffiRPC_struct_t and ALL it's content

int ffiRPC_struct_unlink(ffiRPC_struct_t ffiRPC_struct, char* key); //remove pointer type with key "key" from ffiRPC_struct BUT DOESNT FREE it's data. RETURN 0 on success else 1
int ffiRPC_struct_remove(ffiRPC_struct_t ffiRPC_struct, char* key); //remove type with key "key" from ffiRPC_struct and free it

//=====================================================

int ffiRPC_is_pointer(enum ffiRPC_types type);
void ffiRPC_container_free(struct ffiRPC_container_element* element);

#define CType_to_ffiRPC(Native_type) _Generic((Native_type),                   \
                                    char                 : EffiRPC_char,        \
                                    int8_t               : EffiRPC_int8,        \
                                    uint8_t              : EffiRPC_uint8,       \
                                    int16_t              : EffiRPC_int16,       \
                                    uint16_t             : EffiRPC_uint16,      \
                                    int32_t              : EffiRPC_int32,       \
                                    uint32_t             : EffiRPC_uint32,      \
                                    int64_t              : EffiRPC_int64,       \
                                    uint64_t             : EffiRPC_uint64,      \
                                    float                : EffiRPC_double,      \
                                    double               : EffiRPC_double,      \
                                    char*                : EffiRPC_string,      \
                                    ffiRPC_struct_t      : EffiRPC_struct,      \
                                    default              : EffiRPC_unknown      \
)

#define ffiRPC_cast_value(output, input) typeof(output) cpy = input; output = cpy;

#define C_to_ffiRPC(element,var)({\
    element->type = CType_to_ffiRPC(var);\
    if(ffiRPC_is_pointer(element->type)){\
        void* ptr = NULL;\
        void* cpy_varV = (void*)var;\
        if(element->type == EffiRPC_string) {ptr = strdup(cpy_varV); assert(ptr);}\
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

//==================== public API's ===================

/*Sets a structure element at ffiRPC_struct with type of "input" and value of "input".
 *NOTE: If you are passing string literal you SHOULD cast it to char*
 *NOTE: strings(char*) are always copied when passing into structure
 *NOTE: you can pass void* or any other pointer into the struct but it WONT be serialised
 *NOTE: "key" are always strdup'd
 *
 *EXAMPLE: ffiRPC_struct_set(ffiRPC_struct,"check_int",(uint64_t)12345678);
 *RETURN: 0 on success, else - element exist and you should remove it
*/
#define ffiRPC_struct_set(ffiRPC_struct, key, input)({\
    int __ret = 1;\
    assert(key != NULL);\
    struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct->ht,key);\
    if(element == NULL){\
        element = malloc(sizeof(*element)); assert(element); ffiRPC_struct->size++;\
        C_to_ffiRPC(element,input);\
        hashtable_set(ffiRPC_struct->ht,strdup(key),element);\
        __ret = 0;\
    }(int)(__ret);})

/*Get element from ffiRPC_struct at key "key" and writes it's data into "output". If element "key" does NOT exist returns 1 else 0
 *NOTE: you SHOULD type only NAME of output variable into "output", NOT &output !!!
 *
 *EXAMPLE: uint64_t input = 12345678;
 *         ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
 *         uint64_t output;
 *         assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
 *         assert(output == input);
*/
#define ffiRPC_struct_get(ffiRPC_struct, key, output)({assert(key != NULL);int ret = 1;struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct->ht,key);\
                                                    if(element != NULL){assert(element->type == CType_to_ffiRPC(output));if(ffiRPC_is_pointer(element->type)){ffiRPC_cast_value(output,(typeof(output))element->data);} else{ffiRPC_cast_value(output,*(typeof(output)*)element->data);} ret = 0;}(ret);})

//=====================================================
