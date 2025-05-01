#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "hashtable.c/hashtable.h"

enum ffiRPC_types{
    FFIRPC_char = 1,
    FFIRPC_int8 = 1,
    FFIRPC_uint8,
    FFIRPC_int16,
    FFIRPC_uint16,
    FFIRPC_int32,
    FFIRPC_uint32,
    FFIRPC_int64,
    FFIRPC_uint64,
    FFIRPC_double,

    FFIRPC_string,
    FFIRPC_struct,

    FFIRPC_unknown,
    FFIRPC_duplicate,
};

struct ffiRPC_container_element{
    void* data;
    size_t length;
    enum ffiRPC_types type;
};
typedef struct _ffiRPC_struct *ffiRPC_struct_t;

//==================== public API's ===================

ffiRPC_struct_t ffiRPC_struct_create(void);  //creates a new ffiRPC_struct_t

void ffiRPC_struct_free(ffiRPC_struct_t ffiRPC_struct);  //frees ffiRPC_struct_t and ALL it's content

int ffiRPC_struct_unlink(ffiRPC_struct_t ffiRPC_struct, char* key); //remove pointer type with key "key" from ffiRPC_struct BUT DOESNT FREE it's data. RETURN 0 on success else 1

int ffiRPC_struct_remove(ffiRPC_struct_t ffiRPC_struct, char* key); //remove type with key "key" from ffiRPC_struct and free it.
                                                                    //using removed element is undefined behavior because free will be done on next ffiRPC_struct_set or ffiRPC_struct_free

char* ffiRPC_struct_serialise(ffiRPC_struct_t ffiRPC_struct, size_t* buflen_output); //serialises ffiRPC_struct into char*. Len will be outputed into buflen_output

ffiRPC_struct_t ffiRPC_struct_unserialise(char* buf); //unserialise buf created with ffiRPC_struct_serialise

ffiRPC_struct_t ffiRPC_struct_copy(ffiRPC_struct_t original); //returns a copy of "original"

//=====================================================

int ffiRPC_is_pointer(enum ffiRPC_types type);
void ffiRPC_container_free(struct ffiRPC_container_element* element);
void ffiRPC_struct_cleanup(ffiRPC_struct_t ffiRPC_struct);

void* ffiRPC_struct_HT(ffiRPC_struct_t ffiRPC_struct);
void* ffiRPC_struct_ADF(ffiRPC_struct_t ffiRPC_struct);

#define CType_to_ffiRPC(Native_type) _Generic((Native_type),                   \
                                    char                 : FFIRPC_char,        \
                                    int8_t               : FFIRPC_int8,        \
                                    uint8_t              : FFIRPC_uint8,       \
                                    int16_t              : FFIRPC_int16,       \
                                    uint16_t             : FFIRPC_uint16,      \
                                    int32_t              : FFIRPC_int32,       \
                                    uint32_t             : FFIRPC_uint32,      \
                                    int64_t              : FFIRPC_int64,       \
                                    uint64_t             : FFIRPC_uint64,      \
                                    float                : FFIRPC_double,      \
                                    double               : FFIRPC_double,      \
                                    char*                : FFIRPC_string,      \
                                    ffiRPC_struct_t      : FFIRPC_struct,      \
                                    default              : FFIRPC_unknown      \
)

#define ffiRPC_cast_value(output, input) typeof(output) cpy = input; output = cpy;

#define C_to_ffiRPC(element,var)({\
    element->type = CType_to_ffiRPC(var);\
    if(ffiRPC_is_pointer(element->type)){\
        void* ptr = NULL;\
        void* cpy_varV = (void*)(uint64_t)var;\
        element->length = 0;\
        if(element->type == FFIRPC_string) {ptr = strdup(cpy_varV); assert(ptr); element->length = strlen(ptr) + 1;}\
        else ptr = cpy_varV;\
        element->data = ptr;\
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
    if(ffiRPC_struct->run_GC) ffiRPC_struct_cleanup(ffiRPC_struct);\
    struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct_HT(ffiRPC_struct),key);\
    if(element == NULL){\
        element = malloc(sizeof(*element)); assert(element);\
        C_to_ffiRPC(element,input);\
        hashtable_set(ffiRPC_struct_HT(ffiRPC_struct),strdup(key),element);\
        if(ffiRPC_is_pointer(element->type) && element->type != FFIRPC_string){\
            char NOdoublefree[sizeof(void*) * 2];\
            sprintf(NOdoublefree,"%p",element->data);\
            if(hashtable_get(ffiRPC_struct_ADF(ffiRPC_struct),NOdoublefree) == NULL){\
                struct ffiRPC_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);\
                GC_copy->data = element->data;\
                GC_copy->length = element->length;\
                GC_copy->type = element->type;\
                hashtable_set(ffiRPC_struct_ADF(ffiRPC_struct),strdup(NOdoublefree),GC_copy);\
            }\
        }\
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
#define ffiRPC_struct_get(ffiRPC_struct, key, output)({assert(key != NULL);int ret = 1;struct ffiRPC_container_element* element = hashtable_get(ffiRPC_struct_HT(ffiRPC_struct),key);\
                                                    if(element != NULL){assert(element->type == CType_to_ffiRPC(output));if(ffiRPC_is_pointer(element->type)){ffiRPC_cast_value(output,(typeof(output))element->data);} else{ffiRPC_cast_value(output,*(typeof(output)*)element->data);} ret = 0;}(ret);})

//=====================================================
