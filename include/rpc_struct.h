#pragma once

#include <stdatomic.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "hashtable.h"
#include "rpc_sizedbuf.h"

enum rpc_types{
    RPC_char = 1,
    RPC_int8 = 1,
    RPC_uint8,
    RPC_int16,
    RPC_uint16,
    RPC_int32,
    RPC_uint32,
    RPC_int64,
    RPC_uint64,
    RPC_double,

    RPC_string,
    RPC_struct,
    RPC_sizedbuf,

    RPC_unknown,
    RPC_duplicate,
};

struct rpc_container_element{
    void* data;
    size_t length;
    enum rpc_types type;
};
typedef struct _rpc_struct *rpc_struct_t;

//==================== public API's ===================

rpc_struct_t rpc_struct_create(void);  //creates a new rpc_struct_t

void rpc_struct_free(rpc_struct_t rpc_struct);  //frees rpc_struct_t and ALL it's content

int rpc_struct_unlink(rpc_struct_t rpc_struct, char* key); //remove pointer type with key "key" from rpc_struct BUT DOESNT FREE it's data. RETURN 0 on success else 1

int rpc_struct_remove(rpc_struct_t rpc_struct, char* key); //remove type with key "key" from rpc_struct and free it.
                                                                    //using removed element is undefined behavior because free will be done on next rpc_struct_set or rpc_struct_free

char* rpc_struct_serialise(rpc_struct_t rpc_struct, size_t* buflen_output); //serialises rpc_struct into char*. Len will be outputed into buflen_output

rpc_struct_t rpc_struct_unserialise(char* buf); //unserialise buf created with rpc_struct_serialise

rpc_struct_t rpc_struct_copy(rpc_struct_t original); //returns a copy of "original"

size_t rpc_struct_length(rpc_struct_t rpc_struct); //return length of rpc_struct

char** rpc_struct_getkeys(rpc_struct_t rpc_struct); //return array of char* keys to elements;
enum rpc_types rpc_struct_typeof(rpc_struct_t rpc_struct, char* key); //gets type of element

//=====================================================

int rpc_is_pointer(enum rpc_types type);
void rpc_container_free(struct rpc_container_element* element);
void rpc_struct_cleanup(rpc_struct_t rpc_struct);
size_t rpc_struct_get_runGC(rpc_struct_t rpc_struct);

hashtable* rpc_struct_HT(rpc_struct_t rpc_struct);
hashtable* rpc_struct_ADF(rpc_struct_t rpc_struct);

#define CType_to_rpc(Native_type) _Generic((Native_type),                   \
                                    char                 : RPC_char,        \
                                    int8_t               : RPC_int8,        \
                                    uint8_t              : RPC_uint8,       \
                                    int16_t              : RPC_int16,       \
                                    uint16_t             : RPC_uint16,      \
                                    int32_t              : RPC_int32,       \
                                    uint32_t             : RPC_uint32,      \
                                    int64_t              : RPC_int64,       \
                                    uint64_t             : RPC_uint64,      \
                                    float                : RPC_double,      \
                                    double               : RPC_double,      \
                                    char*                : RPC_string,      \
                                    rpc_struct_t         : RPC_struct,      \
                                    rpc_sizedbuf_t       : RPC_sizedbuf,    \
                                    default              : RPC_unknown      \
)

#define rpc_cast_value(output, input) typeof(output) cpy = (typeof(output))input; output = cpy;

#define C_to_rpc(element,var)({\
    element->type = CType_to_rpc(var);\
    if(rpc_is_pointer(element->type)){\
        void* ptr = NULL;\
        void* cpy_varV = (void*)var;\
        element->length = 0;\
        if(element->type == RPC_string) {ptr = strdup((cpy_varV)); assert(ptr); element->length = strlen(ptr) + 1;}\
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

/*Sets a structure element at rpc_struct with type of "input" and value of "input".
 *NOTE: If you are passing string literal you SHOULD cast it to char*
 *NOTE: strings(char*) are always copied when passing into structure
 *NOTE: you can pass void* or any other pointer into the struct but it WONT be serialised
 *NOTE: "key" are always strdup'd
 *
 *EXAMPLE: rpc_struct_set(rpc_struct,"check_int",(uint64_t)12345678);
 *RETURN: 0 on success, else - element exist and you should remove it
*/
#define rpc_struct_set(rpc_struct, key, input)({\
    int __ret = 1;\
    assert(key != NULL);\
    if(rpc_struct_get_runGC(rpc_struct)) rpc_struct_cleanup(rpc_struct);\
    struct rpc_container_element* element = hashtable_get(rpc_struct_HT(rpc_struct),key);\
    if(element == NULL){\
        element = malloc(sizeof(*element)); assert(element);\
        C_to_rpc(element,input);\
        hashtable_set(rpc_struct_HT(rpc_struct),strdup(key),element);\
        if(rpc_is_pointer(element->type) && element->type != RPC_string){\
            char NOdoublefree[sizeof(void*) * 2];\
            sprintf(NOdoublefree,"%p",element->data);\
            if(hashtable_get(rpc_struct_ADF(rpc_struct),NOdoublefree) == NULL){\
                struct rpc_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);\
                GC_copy->data = element->data;\
                GC_copy->length = element->length;\
                GC_copy->type = element->type;\
                hashtable_set(rpc_struct_ADF(rpc_struct),strdup(NOdoublefree),GC_copy);\
            }\
        }\
        __ret = 0;\
    }(int)(__ret);})

/*Get element from rpc_struct at key "key" and writes it's data into "output". If element "key" does NOT exist returns 1 else 0
 *NOTE: you SHOULD type only NAME of output variable into "output", NOT &output !!!
 *
 *EXAMPLE: uint64_t input = 12345678;
 *         rpc_struct_set(rpc_struct,"check_int",input);
 *         uint64_t output;
 *         assert(rpc_struct_get(rpc_struct,"check_int",output) == 0);
 *         assert(output == input);
*/
#define rpc_struct_get(rpc_struct, key, output)({assert(key != NULL);int ret = 1;struct rpc_container_element* element = hashtable_get(rpc_struct_HT(rpc_struct),key);\
                                                    if(element != NULL){if(rpc_is_pointer(element->type)){rpc_cast_value(output,(typeof(output))element->data);} else{rpc_cast_value(output,*(typeof(output)*)element->data);} ret = 0;}(ret);})

//=====================================================
