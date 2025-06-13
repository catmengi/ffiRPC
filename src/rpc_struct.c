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



#include "../include/rpc_struct.h"
#include "../include/rpc_sizedbuf.h"
#include "../include/rpc_function.h"
#include "../include/hashtable.h"
#include "../include/ptracker.h"
#include "../include/sc_queue.h"

#include <jansson.h>
#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>

#define RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE 16

static struct prec_callbacks rpc_struct_default_prec_cbs;

static char ID_alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";

struct _rpc_struct{
    hashtable* ht;
    char ID[RPC_STRUCT_ID_SIZE];

    rpc_struct_t copyof;
    rpc_struct_destructor manual_destructor;
};

typedef void (*rpc_struct_free_cb)(void*);

typedef struct{
    rpc_struct_t origin;
    char* name;
    rpc_struct_free_cb free;
}prec_rpc_udata;

typedef struct{
    rpc_struct_t* origins;
    int o_index;
    int o_size;

    struct sc_queue_int empty_origins;
}rpc_struct_prec_ctx;

typedef struct{
    hashtable* keys;
    rpc_struct_free_cb free;
}rpc_struct_prec_ptr_ctx;

static inline void rpc_struct_free_internal(rpc_struct_t rpc_struct);

rpc_struct_t rpc_struct_create(void){
    rpc_struct_t rpc_struct = (rpc_struct_t)malloc(sizeof(*rpc_struct));
    assert(rpc_struct);

    rpc_struct->ht = hashtable_create();

    arc4random_buf(rpc_struct->ID,RPC_STRUCT_ID_SIZE - 1);
    rpc_struct->ID[RPC_STRUCT_ID_SIZE - 1] = '\0';

    for(int i = 0 ; i < RPC_STRUCT_ID_SIZE - 1; i++){
        while(rpc_struct->ID[i] == '\0') rpc_struct->ID[i] = arc4random();
        rpc_struct->ID[i] = ID_alphabet[rpc_struct->ID[i] % (sizeof(ID_alphabet) - 1)];
    }

    rpc_struct->copyof = NULL;
    rpc_struct->manual_destructor = NULL;

    return rpc_struct;
}

void rpc_struct_add_destructor(rpc_struct_t rpc_struct, rpc_struct_destructor manual_destructor){
    if(rpc_struct){
        rpc_struct->manual_destructor = manual_destructor;
    }
}
//======================== ptracker callbacks code, aka magic!

static void rpc_struct_onzero_cb(prec_t prec){
    rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
    if(ptr_ctx){
        char** keys = hashtable_get_keys(ptr_ctx->keys);
        size_t length = ptr_ctx->keys->size;
        for(size_t i = 0; i < length; i++){
            rpc_struct_prec_ctx* ctx = hashtable_get(ptr_ctx->keys,keys[i]);
            if(ctx){
                for(int j = 0; j < ctx->o_index; j++){
                    rpc_struct_remove(ctx->origins[j],keys[i]);
                }
                sc_queue_term(&ctx->empty_origins);
                free(ctx->origins);
                free(ctx);
            }
        }
        for(size_t i = 0; i < length; i++) free(keys[i]);
        free(keys);

        if(ptr_ctx->free) ptr_ctx->free(prec_ptr(prec));

        hashtable_destroy(ptr_ctx->keys);
        free(ptr_ctx);
    }
}
static void rpc_struct_increment_cb(prec_t prec, void* udata){
    if(udata){
        prec_rpc_udata* udat = udata;
        rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
        if(ptr_ctx == NULL){
            ptr_ctx = malloc(sizeof(*ptr_ctx)); assert(ptr_ctx);
            ptr_ctx->keys = hashtable_create();
            ptr_ctx->free = udat->free;
            prec_context_set(prec,ptr_ctx);
        }

        rpc_struct_prec_ctx* ctx = hashtable_get(ptr_ctx->keys,udat->name);
        if(ctx == NULL){
            ctx = malloc(sizeof(*ctx)); assert(ctx);

            ctx->o_index = 0;
            ctx->o_size = RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE;
            ctx->origins = malloc(sizeof(*ctx->origins) * ctx->o_size); assert(ctx->origins);
            sc_queue_init(&ctx->empty_origins);
            hashtable_set(ptr_ctx->keys,strdup(udat->name),ctx);
        }
        int index = (sc_queue_size(&ctx->empty_origins) == 0 ? ctx->o_index++ : sc_queue_del_first(&ctx->empty_origins));
        if(index == ctx->o_size - 1) assert((ctx->origins = realloc(ctx->origins, sizeof(*ctx->origins) * (ctx->o_size += RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE))));
        ctx->origins[index] = udat->origin;
    }
}
static void rpc_struct_decrement_cb(prec_t prec, void* udata){
    if(udata){
        rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
        if(ptr_ctx){
            prec_rpc_udata* udat = udata;
            rpc_struct_prec_ctx* ctx = hashtable_get(ptr_ctx->keys,udat->name);
            if(ctx){
                for(int i = 0; i < ctx->o_index; i++){
                    if(ctx->origins[i] == udat->origin){
                        ctx->origins[i] = NULL;

                        if(i == ctx->o_index - 1) ctx->o_index--;
                        else sc_queue_add_last(&ctx->empty_origins,i);
                    }
                }
                if(ctx->o_index == 0){
                    char* kfree = ptr_ctx->keys->body[hashtable_find_slot(ptr_ctx->keys,udat->name)].key;
                    hashtable_remove(ptr_ctx->keys,kfree);
                    free(kfree);

                    sc_queue_term(&ctx->empty_origins);
                    free(ctx->origins);
                    free(ctx);
                }
            }
        }
    }
}

static struct prec_callbacks rpc_struct_default_prec_cbs = {
    .zero = rpc_struct_onzero_cb,
    .increment = rpc_struct_increment_cb,
    .decrement = rpc_struct_decrement_cb,
};

//=====================================================

static void rpc_struct_free_internal(rpc_struct_t rpc_struct){
    if(rpc_struct){
        if(rpc_struct->manual_destructor) rpc_struct->manual_destructor(rpc_struct);
        for(size_t i = 0; i < rpc_struct->ht->capacity; i++){
            if(rpc_struct->ht->body[i].key != NULL && rpc_struct->ht->body[i].key != (char*)0xDEAD){
                rpc_struct_remove(rpc_struct,rpc_struct->ht->body[i].key);
            }
        }

        hashtable_destroy(rpc_struct->ht);
        free(rpc_struct);
    }
}

void rpc_struct_free(rpc_struct_t rpc_struct){
    if(rpc_struct){
        prec_t prec = prec_get(rpc_struct);
        if(prec) prec_delete(prec);
        else rpc_struct_free_internal(rpc_struct);
    }
}

char* rpc_struct_id_get(rpc_struct_t rpc_struct){
    return rpc_struct->ID;
}

void rpc_struct_id_set(rpc_struct_t rpc_struct, char ID[RPC_STRUCT_ID_SIZE]){
    memcpy(rpc_struct->ID,ID,RPC_STRUCT_ID_SIZE);
}

int rpc_is_pointer(enum rpc_types type){ //return 1 if rpc_type is pointer, 0 if not
    int ret = 0;
    if(type == RPC_struct || type == RPC_string || type == RPC_sizedbuf || type == RPC_function || type == RPC_unknown) ret = 1;

    return ret;
}
static rpc_struct_free_cb rpc_freefn_of(enum rpc_types type){
    rpc_struct_free_cb fn = NULL;
    if(rpc_is_pointer(type)){
        switch(type){
            case RPC_struct:
                fn = (rpc_struct_free_cb)rpc_struct_free_internal; //since this function is intended only for internal usage in prec callbacks
                break;
            case RPC_sizedbuf:
                fn = (rpc_struct_free_cb)rpc_sizedbuf_free;
                break;
            case RPC_function:
                fn = (rpc_struct_free_cb)rpc_function_free;

            default: break;
        }
    }
    return fn;
}

int rpc_struct_remove(rpc_struct_t rpc_struct, char* key){
    if(rpc_struct && key){
        struct rpc_container_element* element = hashtable_get(rpc_struct->ht,key);
        if(element){
            if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
                prec_rpc_udata udat = {
                    .name = key,
                    .origin = rpc_struct,
                };
                prec_decrement(prec_get(element->data),&udat);
            } else if(element->type == RPC_string || !rpc_is_pointer(element->type)) free(element->data);

            char* kfree = rpc_struct->ht->body[hashtable_find_slot(rpc_struct->ht,key)].key;
            hashtable_remove(rpc_struct->ht,key);
            free(kfree);
            free(element);
            return 0;
        }
        return 1;
    }
    return 1;
}

struct rpc_serialise_element{
    char* buf;
    size_t buflen;
    enum rpc_types type;

    char* key;
};
struct rpc_struct_duplicate_info{
    enum rpc_types type;
    char* original_name;

    size_t duplicates_len;
    char** duplicates;
};

struct rpc_struct_duplicate_info* rpc_struct_found_duplicates(rpc_struct_t rpc_struct, size_t* len_output){
    prec_t* all = prec_get_all(len_output);
    struct rpc_struct_duplicate_info* duplicates = NULL;

    if(*len_output > 0){
        duplicates = calloc(*len_output,sizeof(*duplicates)); assert(duplicates);
        char** keys = rpc_struct_keys(rpc_struct);

        for(size_t i = 0; i < *len_output; i++){
            //STEP 1: find first name with matching ptr
            for(size_t j = 0; j < rpc_struct_length(rpc_struct); j++){
                struct rpc_container_element* element = hashtable_get(rpc_struct->ht,keys[j]);
                if(element->data == prec_ptr(all[i])){
                    if(duplicates[i].original_name == NULL){
                        duplicates[i].original_name = keys[j];
                        duplicates[i].type = element->type;
                    } else duplicates[i].duplicates_len++;
                }
            }

            //STEP 2: find duplicates now
            size_t DI = 0;
            duplicates[i].duplicates = calloc(duplicates[i].duplicates_len, sizeof(char*)); assert(duplicates[i].duplicates);
            for(size_t j = 0; j < rpc_struct_length(rpc_struct); j++){
                struct rpc_container_element* element = hashtable_get(rpc_struct->ht,keys[j]);
                if(element->data == prec_ptr(all[i])){
                    if(strcmp(duplicates[i].original_name,keys[j]) != 0){
                        duplicates[i].duplicates[DI++] = keys[j];
                    }
                }
            }
        }
        free(keys);
    }
    free(all);
    return duplicates;
}

#define STRINGIFY(x) #x
json_t* rpc_struct_serialise(rpc_struct_t rpc_struct){
    hashtable* dupless_ht = malloc(sizeof(*dupless_ht)); assert(dupless_ht);

    dupless_ht->size = rpc_struct->ht->size;
    dupless_ht->capacity = rpc_struct->ht->capacity;
    assert(pthread_mutex_init(&dupless_ht->lock,NULL) == 0);
    dupless_ht->body = malloc(dupless_ht->capacity * sizeof(*dupless_ht->body)); assert(dupless_ht);
    memcpy(dupless_ht->body,rpc_struct->ht->body,sizeof(hashtable_entry) * dupless_ht->capacity);

    size_t dups_len = 0;
    struct rpc_struct_duplicate_info* dups = rpc_struct_found_duplicates(rpc_struct,&dups_len);

    size_t skip_items = 0;
    char** keys = rpc_struct_keys(rpc_struct); //get keys for removing RPC_unknown!
    for(size_t i = 0; i < rpc_struct_length(rpc_struct); i++){
        struct rpc_container_element* element = hashtable_get(dupless_ht,keys[i]);
        if(element->type == RPC_unknown){
            hashtable_remove(dupless_ht, keys[i]);
            skip_items++;
        }
    }
    free(keys);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            hashtable_remove(dupless_ht,dups[i].duplicates[j]);
        }
    }

    json_t* root = json_object(); assert(root);
    json_object_set_new(root, "ID", json_string(rpc_struct->ID));
    json_object_set_new(root, "type", json_string(STRINGIFY(RPC_struct)));

    json_t* serialised = json_object();
    json_object_set_new(root,"serialised",serialised);

    char** dupless_keys = hashtable_get_keys(dupless_ht);
    for(size_t i = 0; i < dupless_ht->size; i++){
        struct rpc_container_element* el = hashtable_get(dupless_ht, dupless_keys[i]);
        json_t* item = NULL;
        if(rpc_is_pointer(el->type) && el->type != RPC_string){
            switch(el->type){
                case RPC_struct:
                    item = rpc_struct_serialise(el->data);
                    break;
                case RPC_function:
                    item = rpc_function_serialise(el->data);
                    break;
                case RPC_sizedbuf:
                    item = rpc_sizedbuf_serialise(el->data);
                    break;
                default: break;
            }
        } else {
            switch(el->type){
                case RPC_number:{
                        json_int_t json_int = 0;
                        switch(el->length){
                            case sizeof(uint8_t):
                                json_int = *(uint8_t*)el->data;
                                break;
                            case sizeof(uint16_t):
                                json_int = *(uint16_t*)el->data;
                                break;
                            case sizeof(uint32_t):
                                json_int = *(uint32_t*)el->data;
                                break;
                            case sizeof(uint64_t):
                                json_int = *(uint64_t*)el->data;
                                break;
                        }
                        item = json_integer(json_int);
                    }
                    break;
                case RPC_real:{
                        double json_double = 0;
                        switch(el->length){
                            case sizeof(float):
                                json_double = *(float*)el->data;
                                break;
                            case sizeof(double):
                                json_double = *(double*)el->data;
                                break;
                        }
                        item = json_real(json_double);
                    }
                    break;
                case RPC_string:
                    item = json_string(el->data);
                    break;
                default: break;
            }
        }
        json_object_set_new(serialised,dupless_keys[i], item);
    }
    free(dupless_keys);
    hashtable_destroy(dupless_ht);

    json_t* duplicates = json_object();
    json_object_set_new(root,"duplicates",duplicates);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            json_object_set_new(duplicates,dups[i].duplicates[j],json_string(dups[i].original_name));
        }
        free(dups[i].duplicates);
    }
    free(dups);

    return root;
}
static void item_parse(json_t* item, rpc_struct_t rpc_struct, char* key){
    switch(json_typeof(item)){
        case JSON_INTEGER:
            rpc_struct_set(rpc_struct,key, json_number_value(item));
            break;

        case JSON_REAL:
            rpc_struct_set(rpc_struct, key, json_real_value(item));
            break;

        case JSON_STRING:
            rpc_struct_set(rpc_struct,key,json_string_value(item));
            break;

        case JSON_OBJECT:{ //using braces due to variable declaration!
            const char* item_type = json_string_value(json_object_get(item,"type"));

            if(strcmp(item_type, STRINGIFY(RPC_struct)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_struct_unserialise(item));
            } else if(strcmp(item_type, STRINGIFY(RPC_function)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_function_unserialise(item));

            } else if(strcmp(item_type, STRINGIFY(RPC_sizedbuf)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_sizedbuf_unserialise(item));
            }
        }
        break;

        case JSON_ARRAY:{
            json_t* array_item = NULL;
            char arr_key[sizeof(size_t) * 4];
            size_t i = 0;

            rpc_struct_t arr_s = rpc_struct_create();
            rpc_struct_set(rpc_struct, key, arr_s);

            json_array_foreach(item,i,array_item){
                sprintf(arr_key, "%zu",i);
                item_parse(array_item,arr_s,arr_key);
            }
        }
    }
}
rpc_struct_t rpc_struct_unserialise(json_t* json){
    rpc_struct_t new = rpc_struct_create();

    json_t* ID = json_object_get(json,"ID");
    if(ID){
        rpc_struct_id_set(new,(char*)json_string_value(ID));
    }

    json_t* type = json_object_get(json,"type");
    if(type == NULL || strcmp(json_string_value(type), STRINGIFY(RPC_struct)) != 0) goto bad_exit;

    json_t* data = json_object_get(json,"serialised");
    if(data){
        json_t* item = NULL;
        const char* key = NULL;
        json_object_foreach(data,key,item){
            item_parse(item, new, (char*)key);
        }

    } else goto bad_exit;

    json_t* duplicates = json_object_get(json,"duplicates");
    if(duplicates){
        json_t* item = NULL;
        const char* key = NULL;
        json_object_foreach(duplicates,key,item){
            const char* original = json_string_value(item);

            struct rpc_container_element* original_cont = rpc_struct_get_internal(new,(char*)original);
            assert(original_cont);

            struct rpc_container_element* dup_cont = malloc(sizeof(*dup_cont)); assert(dup_cont);

            *dup_cont = *original_cont;

            rpc_struct_set_internal(new,(char*)key,dup_cont);
        }
    }

    return new;

bad_exit:
    rpc_struct_free(new);
    return NULL;
}

#define copy(input) ({void* __out = malloc(sizeof(*input)); assert(__out); memcpy(__out,input,sizeof(*input)); (__out);})

rpc_struct_t rpc_struct_copy(rpc_struct_t original){
    rpc_struct_t copy = rpc_struct_create();

    copy->ht->capacity = original->ht->capacity;
    copy->ht->size = original->ht->size;
    assert((copy->ht->body = realloc(copy->ht->body,copy->ht->capacity * sizeof(*copy->ht->body))));
    memcpy(copy->ht->body,original->ht->body,sizeof(*copy->ht->body) * copy->ht->capacity);

    for(size_t i = 0; i < copy->ht->capacity; i++){
        if(copy->ht->body[i].key != NULL && copy->ht->body[i].key != (void*)0xDEAD){
            copy->ht->body[i].key = strdup(copy->ht->body[i].key); //recoping keys because it WILL cause double-free if we not done this
            copy->ht->body[i].value = copy((struct rpc_container_element*)copy->ht->body[i].value);

            struct rpc_container_element* element = copy->ht->body[i].value;
            if(!rpc_is_pointer(element->type) || element->type == RPC_string){
                void* tmp = malloc(element->length); assert(tmp);
                memcpy(tmp,element->data,element->length);
                element->data = tmp;
            } else if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
                prec_rpc_udata udat = {
                    .name = copy->ht->body[i].key,
                    .origin = copy,
                    .free = rpc_freefn_of(element->type),
                };
                prec_increment(prec_get(element->data),&udat);
            }
        }
    }
    copy->copyof = original;
    return copy;
}

rpc_struct_t rpc_struct_whoose_copy(rpc_struct_t rpc_struct){
    rpc_struct_t parent = rpc_struct->copyof;
    if(rpc_struct->copyof != NULL){
        prec_t parent_prec = prec_get(rpc_struct->copyof);
        if(parent_prec == NULL){ //object no longer exist or parent rpc_struct is NOT TRACKER EVERYWHERE ELSE?
            rpc_struct->copyof = NULL;
            parent = NULL;
        }
    }
    return parent;
}

size_t rpc_struct_length(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return rpc_struct->ht->size;
}
char** rpc_struct_keys(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return hashtable_get_keys(rpc_struct->ht);
}

enum rpc_types rpc_struct_typeof(rpc_struct_t rpc_struct, char* key){
    assert(rpc_struct);
    struct rpc_container_element* element = hashtable_get(rpc_struct->ht,key);
    return (element == NULL ? 0 : element->type);
}

int rpc_struct_exist(rpc_struct_t rpc_struct, char* key){
    return (hashtable_get(rpc_struct->ht,key) == NULL ? 0 : 1);
}

int rpc_struct_set_internal(rpc_struct_t rpc_struct, char* key, struct rpc_container_element* element){
    if(element->data == NULL) {free(element); return 1;}

    if(hashtable_get(rpc_struct->ht,key) == NULL){
        if(element->type == RPC_string){
            element->data = strdup(element->data); assert(element->data);
            element->length = strlen(element->data) + 1;
        }
        if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
            prec_t prec = prec_get(element->data);
            if(prec == NULL) prec = prec_new(element->data,rpc_struct_default_prec_cbs);

            prec_rpc_udata udat = {
                .name = key,
                .origin = rpc_struct,
                .free = rpc_freefn_of(element->type),
            };
            prec_increment(prec,&udat);
        }
        hashtable_set(rpc_struct->ht,strdup(key),element);
        return 0;
    }
    return 1;
}

struct rpc_container_element* rpc_struct_get_internal(rpc_struct_t rpc_struct, char* key){
    return hashtable_get(rpc_struct->ht,key);
}

void rpc_struct_increment_refcount(void* ptr){
    prec_t prec = prec_get(ptr);
    if(prec == NULL) prec = prec_new(ptr,rpc_struct_default_prec_cbs);

    prec_increment(prec,NULL);
}
void rpc_struct_decrement_refcount(void* ptr){
    prec_t prec = prec_get(ptr);
    if(prec) prec_decrement(prec,NULL);
}

uint64_t rpc_struct_hash(rpc_struct_t rpc_struct){
    uint64_t hash = 0;
    char** keys = rpc_struct_keys(rpc_struct);
    for(size_t i = 0; i < rpc_struct_length(rpc_struct); i++){
        struct rpc_container_element* element = hashtable_get(rpc_struct->ht,keys[i]);
        uint64_t new_hash = hash;
        if(rpc_is_pointer(element->type) && element->type != RPC_string){
            switch(element->type){
                case RPC_struct:
                    new_hash += rpc_struct_hash(element->data);
                    break;
                case RPC_sizedbuf:
                    new_hash += rpc_sizedbuf_hash(element->data);
                    break;
                default: new_hash += murmur(element->data,sizeof(element->data)); break;
            }
        } else new_hash += murmur(element->data,element->length);

        hash = murmur((uint8_t*)&new_hash,sizeof(new_hash));
    }
    free(keys);
    return hash;
}
