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
#include "../include/hashtable.h"
#include "../include/ptracker.h"
#include "../include/sc_queue.h"

#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>

#define RPC_STRUCT_SERIALISE_IDENT "ffiRPC v0 format version!"

size_t rpctype_sizes[RPC_duplicate] = {
    0,
    sizeof(char),
    sizeof(uint8_t), sizeof(int16_t),
    sizeof(uint16_t), sizeof(int32_t),
    sizeof(uint32_t), sizeof(int64_t),
    sizeof(uint64_t), sizeof(double),
    0,0,0,0
};

struct _rpc_struct{
    hashtable* ht;
};

typedef struct{
    rpc_struct_t origin;
    char* name;
}prec_rpc_udata;

#define RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE 16
typedef struct{
    rpc_struct_t* origins;
    int o_index;
    int o_size;

    struct sc_queue_int empty_origins;
}rpc_struct_prec_ctx;

static inline void rpc_struct_free_internal(rpc_struct_t rpc_struct);

rpc_struct_t rpc_struct_create(void){
    rpc_struct_t rpc_struct = (rpc_struct_t)malloc(sizeof(*rpc_struct));
    assert(rpc_struct);

    rpc_struct->ht = hashtable_create();

    return rpc_struct;
}
//========================

static void rpc_struct_onzero_cb(prec_t prec){
    hashtable* ht = prec_context_get(prec);
    if(ht){
        char** keys = hashtable_get_keys(ht);
        size_t length = ht->size;
        for(size_t i = 0; i < length; i++){
            rpc_struct_prec_ctx* ctx = hashtable_get(ht,keys[i]);
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
        hashtable_destroy(ht);
        rpc_struct_free_internal(prec_ptr(prec));
    }
}
static void rpc_struct_increment_cb(prec_t prec, void* udata){
    if(udata){
        prec_rpc_udata* udat = udata;
        hashtable* ht = (prec_context_get(prec) != NULL ? prec_context_get(prec) : hashtable_create());
        if(prec_context_get(prec) == NULL) prec_context_set(prec,ht);

        rpc_struct_prec_ctx* ctx = hashtable_get(ht,udat->name);
        if(ctx == NULL){
            ctx = malloc(sizeof(*ctx)); assert(ctx);

            ctx->o_index = 0;
            ctx->o_size = RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE;
            ctx->origins = malloc(sizeof(*ctx->origins) * ctx->o_size); assert(ctx->origins);
            sc_queue_init(&ctx->empty_origins);
            hashtable_set(ht,strdup(udat->name),ctx);
        }
        int index = (sc_queue_size(&ctx->empty_origins) == 0 ? ctx->o_index++ : sc_queue_del_first(&ctx->empty_origins));
        if(index == ctx->o_size - 1) assert((ctx->origins = realloc(ctx->origins, sizeof(*ctx->origins) * (ctx->o_size += RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE))));
        ctx->origins[index] = udat->origin;
    }
}
static void rpc_struct_decrement_cb(prec_t prec, void* udata){
    if(udata){
        hashtable* ht = prec_context_get(prec);
        if(ht){
            prec_rpc_udata* udat = udata;
            rpc_struct_prec_ctx* ctx = hashtable_get(ht,udat->name);
            if(ctx){
                for(int i = 0; i < ctx->o_index; i++){
                    if(ctx->origins[i] == udat->origin){
                        ctx->origins[i] = NULL;

                        if(i == ctx->o_index - 1) ctx->o_index--;
                        else sc_queue_add_last(&ctx->empty_origins,i);
                    }
                }
                if(ctx->o_index == 0){
                    char* kfree = ht->body[hashtable_find_slot(ht,udat->name)].key;
                    hashtable_remove(ht,kfree);
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

static void rpc_struct_free_internal(rpc_struct_t rpc_struct){
    if(rpc_struct){
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

int rpc_is_pointer(enum rpc_types type){ //return 1 if rpc_type is pointer, 0 if not
    int ret = 0;
    if(type == RPC_struct || type == RPC_string || type == RPC_sizedbuf ||type == RPC_unknown) ret = 1;

    return ret;
}

void rpc_container_free(struct rpc_container_element* element){
    if(rpc_is_pointer(element->type) && element->type != RPC_string){
        switch(element->type){
            case RPC_struct:
                rpc_struct_free(element->data);
                break;
            case RPC_sizedbuf:
                rpc_sizedbuf_free(element->data);
                break;
            default: break; //rpc_unknown should not be freed
        }
    } else free(element->data);
}

int rpc_struct_remove(rpc_struct_t rpc_struct, char* key){
    static int log_call = 0;
    if(rpc_struct && key){
        struct rpc_container_element* element = hashtable_get(rpc_struct->ht,key);
        if(element){
            log_call++;
            if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
                prec_rpc_udata udat = {
                    .name = key,
                    .origin = rpc_struct,
                };
                prec_decrement(prec_get(element->data),&udat);
            } else if(element->type == RPC_string || !rpc_is_pointer(element->type)) rpc_container_free(element);

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

char* rpc_struct_serialise(rpc_struct_t rpc_struct, size_t* buflen_output){
    hashtable* dupless_ht = malloc(sizeof(*dupless_ht)); assert(dupless_ht);

    dupless_ht->size = rpc_struct->ht->size;
    dupless_ht->capacity = rpc_struct->ht->capacity;
    assert(pthread_mutex_init(&dupless_ht->lock,NULL) == 0);
    dupless_ht->body = malloc(dupless_ht->capacity * sizeof(*dupless_ht->body)); assert(dupless_ht);
    memcpy(dupless_ht->body,rpc_struct->ht->body,sizeof(hashtable_entry) * dupless_ht->capacity);

    size_t dups_len = 0; size_t serialise_elements_len = rpc_struct->ht->size; //we should have space for dublicates but they will be serialised in other manner
    struct rpc_struct_duplicate_info* dups = rpc_struct_found_duplicates(rpc_struct,&dups_len);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            hashtable_remove(dupless_ht,dups[i].duplicates[j]);
        }
    }

    size_t PS_index = 0;
    char** dupless_keys = hashtable_get_keys(dupless_ht);
    struct rpc_serialise_element* pre_serialise = calloc(serialise_elements_len,sizeof(*pre_serialise)); assert(pre_serialise);
    for(size_t i = 0; i < dupless_ht->size; i++){
        struct rpc_container_element* element = hashtable_get(dupless_ht,dupless_keys[i]);
        pre_serialise[PS_index].type = element->type;
        pre_serialise[PS_index].key = dupless_keys[i];
        if(rpc_is_pointer(element->type)){
            switch(element->type){
                case RPC_struct:
                    pre_serialise[PS_index].buf = rpc_struct_serialise(element->data,&pre_serialise[PS_index].buflen);
                    break;
                case RPC_sizedbuf:
                    pre_serialise[PS_index].buf = rpc_sizedbuf_serialise(element->data,&pre_serialise[PS_index].buflen);
                    break;
                case RPC_string:
                    pre_serialise[PS_index].buf = element->data;
                    pre_serialise[PS_index].buflen = strlen(element->data) + 1;
                    break;
                default: break;
            }
        } else {pre_serialise[PS_index].buf = element->data; pre_serialise[PS_index].buflen = element->length;}
        PS_index++;
    }
    free(dupless_keys);
    hashtable_destroy(dupless_ht);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            pre_serialise[PS_index].type = RPC_duplicate;
            pre_serialise[PS_index].key = dups[i].duplicates[j];
            pre_serialise[PS_index].buf = dups[i].original_name;
            pre_serialise[PS_index].buflen = strlen(dups[i].original_name) + 1;
            PS_index++;
        }
        free(dups[i].duplicates);
    }
    free(dups);

    uint64_t real_len = 0;
    size_t final_buflen = sizeof(uint64_t) + sizeof(RPC_STRUCT_SERIALISE_IDENT);
    for(size_t i = 0; i < serialise_elements_len; i++){
        if(pre_serialise[i].key == NULL) break;

        final_buflen += strlen(pre_serialise[i].key) + 1; //key
        final_buflen += 1; //type
        final_buflen += sizeof(uint64_t); //length of payload length
        final_buflen += pre_serialise[i].buflen; //payload length
        real_len++;
    }

    char* buf = malloc(final_buflen); assert(buf);

    memcpy(buf,RPC_STRUCT_SERIALISE_IDENT,sizeof(RPC_STRUCT_SERIALISE_IDENT));

    char* write_buf = buf + sizeof(RPC_STRUCT_SERIALISE_IDENT);

    memcpy(write_buf,&real_len,sizeof(uint64_t)); write_buf += sizeof(uint64_t);

    *buflen_output = final_buflen;
    for(uint64_t i = 0; i < real_len; i++){
        uint64_t u64_buflen = pre_serialise[i].buflen;
        memcpy(write_buf,pre_serialise[i].key, strlen(pre_serialise[i].key) + 1); write_buf += (strlen(pre_serialise[i].key) + 1);
        *write_buf = pre_serialise[i].type; write_buf++;

        memcpy(write_buf,&u64_buflen,sizeof(uint64_t)); write_buf += sizeof(uint64_t);
        memcpy(write_buf,pre_serialise[i].buf,pre_serialise[i].buflen); write_buf += pre_serialise[i].buflen;

        if(rpc_is_pointer(pre_serialise[i].type) && pre_serialise[i].type != RPC_string){
            switch(pre_serialise[i].type){
                case RPC_struct:
                    free(pre_serialise[i].buf);
                    break;
                case RPC_sizedbuf:
                    free(pre_serialise[i].buf);
                    break;
                default: break;
            }
        }
    }
    free(pre_serialise);
    return buf;
}
rpc_struct_t rpc_struct_unserialise(char* buf){
    assert(buf);

    if((sizeof(RPC_STRUCT_SERIALISE_IDENT) - 1 != strlen(buf)) || (memcmp(buf,RPC_STRUCT_SERIALISE_IDENT,strlen(buf)) != 0)) //try to not compare outside ident string
        return NULL;

    buf += strlen(buf) + 1;

    uint64_t u64_parse_len = 0;
    memcpy(&u64_parse_len,buf,sizeof(uint64_t)); buf += sizeof(uint64_t);

    rpc_struct_t new = rpc_struct_create();
    struct rpc_serialise_element serialise = {0};

    for(uint64_t i = 0; i < u64_parse_len; i++){
        serialise.key = buf; buf += strlen(serialise.key) + 1;
        serialise.type = *buf; buf++;

        uint64_t U64_buflen = 0;
        memcpy(&U64_buflen,buf,sizeof(uint64_t)); buf += sizeof(uint64_t);
        serialise.buflen = U64_buflen;

        serialise.buf = buf;

        buf += serialise.buflen;
        if(rpc_is_pointer(serialise.type) && serialise.type != RPC_string){
            void* unserialised = NULL;
            switch(serialise.type){
                case RPC_struct:
                    unserialised = rpc_struct_unserialise(serialise.buf);
                    break;
                case RPC_sizedbuf:
                    unserialised = rpc_sizedbuf_unserialise(serialise.buf);
                    break;
                default: break; //UNKNOWN TYPE, NEED TO DIE
            }

            struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);

            element->data = unserialised;
            element->type = serialise.type;
            element->length = 0;

            prec_rpc_udata udat = {
                .name = serialise.key,
                .origin = new,
            };
            prec_increment(prec_new(unserialised,rpc_struct_default_prec_cbs),&udat);

            hashtable_set(new->ht,strdup(serialise.key),element);

        } else if(serialise.type != RPC_duplicate){
            struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);

            element->type = serialise.type;

            element->data = malloc(serialise.buflen); assert(serialise.buflen);
            memcpy(element->data,serialise.buf,serialise.buflen);

            element->length = serialise.buflen;

            hashtable_set(new->ht,strdup(serialise.key),element);
        } else if(serialise.type == RPC_duplicate){
            struct rpc_container_element* element = hashtable_get(new->ht,serialise.buf); //using buf because in buf we wrote "key" of original
            assert(element);

            struct rpc_container_element* DUP_element = malloc(sizeof(*DUP_element)); assert(DUP_element);
            *DUP_element = *element;

            prec_rpc_udata udat = {
                .name = serialise.key,
                .origin = new,
            };
            prec_increment(prec_get(element->data),&udat);

            hashtable_set(new->ht,strdup(serialise.key),DUP_element);

        }
    }
    return new;
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
                };
                prec_increment(prec_get(element->data),&udat);
            }
        }
    }
    return copy;
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
                .origin = rpc_struct
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
