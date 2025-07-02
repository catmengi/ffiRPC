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


#include <ffirpc/rpc_config.h>
#include <ffirpc/rpc_struct_internal.h>
#include <ffirpc/rpc_struct.h>
#include <ffirpc/hashmap/hashmap.h>
#include <ffirpc/rpc_sizedbuf.h>
#include <ffirpc/rpc_function.h>
#include <ffirpc/ptracker.h>
#include <ffirpc/sc_queue.h>

#ifdef RPC_SERIALISERS
#include <jansson.h>
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#define RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE 16

struct prec_callbacks rpc_struct_default_prec_cbs;
char ID_alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";

struct _rpc_struct{
    HASHMAP(char,rpc_type_t) map;
    pthread_mutex_t lock;

    char ID[RPC_STRUCT_ID_SIZE];

    rpc_struct_t copyof;
    rpc_struct_destructor manual_destructor;
};


typedef struct{
    rpc_struct_t* origins;
    int o_index;
    int o_size;

    struct sc_queue_int empty_origins;
}rpc_struct_prec_ctx;

uint64_t murmur(uint8_t* inbuf,uint32_t keylen){
    uint64_t h = (525201411107845655ull);
    for (uint32_t i =0; i < keylen; i++,inbuf++){
        h ^= *inbuf;
        h *= 0x5bd1e9955bd1e995;
        h ^= h >> 47;
    }
    return h;
}

static inline void rpc_struct_free_internal(rpc_struct_t rpc_struct);

rpc_struct_t rpc_struct_create(void){
    rpc_struct_t rpc_struct = (rpc_struct_t)malloc(sizeof(*rpc_struct));
    assert(rpc_struct);

    hashmap_init(&rpc_struct->map,hashmap_hash_string,strcmp);
    hashmap_set_key_alloc_funcs(&rpc_struct->map,strdup,(void (*)(char*))free);

    arc4random_buf(rpc_struct->ID,RPC_STRUCT_ID_SIZE - 1);
    rpc_struct->ID[RPC_STRUCT_ID_SIZE - 1] = '\0';

    for(int i = 0 ; i < RPC_STRUCT_ID_SIZE - 1; i++){
        while(rpc_struct->ID[i] == '\0') rpc_struct->ID[i] = arc4random();
        rpc_struct->ID[i] = ID_alphabet[rpc_struct->ID[i] % (sizeof(ID_alphabet) - 1)];
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&rpc_struct->lock,&attr);

    rpc_struct->copyof = NULL;
    rpc_struct->manual_destructor = NULL;

    return rpc_struct;
}

void rpc_struct_add_destructor(rpc_struct_t rpc_struct, rpc_struct_destructor manual_destructor){
    if(rpc_struct){
        rpc_struct->manual_destructor = manual_destructor;
    }
}

INTERNAL_API size_t rpc_struct_memsize(){
    return sizeof(struct _rpc_struct);
}
//======================== ptracker callbacks code, aka magic!
void rpc_struct_prec_ctx_destroy(prec_t prec){ //not static because i need this one it client's code for argument updating
    rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
    if(ptr_ctx){
        pthread_mutex_lock(&ptr_ctx->lock);
        if(ptr_ctx->free) ptr_ctx->free(prec_ptr(prec));

        hashmap_cleanup(&ptr_ctx->keys);
        prec_context_set(prec, NULL);
        pthread_mutex_unlock(&ptr_ctx->lock);
        pthread_mutex_destroy(&ptr_ctx->lock);
        free(ptr_ctx);
    }
}
static void rpc_struct_onzero_cb(prec_t prec){
    rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
    if(ptr_ctx){
        const char* foreach_key = NULL;
        rpc_struct_prec_ctx* ctx = NULL;
        void* pos = NULL;
        hashmap_foreach_safe(foreach_key,ctx,&ptr_ctx->keys, pos){
            if(ctx){
                for(int j = 0; j < ctx->o_index; j++){
                    if(ctx->origins[j]){
                        pthread_mutex_lock(&ctx->origins[j]->lock);
                        rpc_struct_remove(ctx->origins[j], (char*)foreach_key);
                        pthread_mutex_unlock(&ctx->origins[j]->lock);
                    }
                }
                sc_queue_term(&ctx->empty_origins);
                free(ctx->origins);
                free(ctx);
            }
        }
    }
    rpc_struct_prec_ctx_destroy(prec);
}
static rpc_struct_prec_ptr_ctx* prec_ptr_ctx_create_or_get(prec_t prec, prec_rpc_udata* udat){
    rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
    if(ptr_ctx == NULL){
        ptr_ctx = malloc(sizeof(*ptr_ctx)); assert(ptr_ctx);

        hashmap_init(&ptr_ctx->keys,hashmap_hash_string, strcmp);
        hashmap_set_key_alloc_funcs(&ptr_ctx->keys,strdup,(void (*)(char*))free);
        ptr_ctx->free = udat->free;

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&ptr_ctx->lock,&attr);
        prec_context_set(prec,ptr_ctx);
    }
    return ptr_ctx;
}
static void rpc_struct_increment_cb(prec_t prec, void* udata){
    if(udata){
        prec_rpc_udata* udat = udata;
        rpc_struct_prec_ptr_ctx* ptr_ctx = prec_ptr_ctx_create_or_get(prec,udat);

        pthread_mutex_lock(&ptr_ctx->lock);
        if(udat->name != NULL && udat->origin != NULL){
            pthread_mutex_lock(&udat->origin->lock);
            rpc_struct_prec_ctx* ctx = hashmap_get(&ptr_ctx->keys, udat->name);
            if(ctx == NULL){
                ctx = malloc(sizeof(*ctx)); assert(ctx);

                ctx->o_index = 0;
                ctx->o_size = RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE;
                ctx->origins = malloc(sizeof(*ctx->origins) * ctx->o_size); assert(ctx->origins);
                sc_queue_init(&ctx->empty_origins);
                hashmap_put(&ptr_ctx->keys,udat->name,ctx);
            }
            int index = (sc_queue_size(&ctx->empty_origins) == 0 ? ctx->o_index++ : sc_queue_del_first(&ctx->empty_origins));
            if(index == ctx->o_size - 1) assert((ctx->origins = realloc(ctx->origins, sizeof(*ctx->origins) * (ctx->o_size += RPC_STRUCT_PREC_CTX_DEFAULT_ORIGINS_SIZE))));
            ctx->origins[index] = udat->origin;
            pthread_mutex_unlock(&udat->origin->lock);
        }
        pthread_mutex_unlock(&ptr_ctx->lock);
    }
}
static void rpc_struct_decrement_cb(prec_t prec, void* udata){
    if(udata){
        rpc_struct_prec_ptr_ctx* ptr_ctx = prec_context_get(prec);
        if(ptr_ctx){
            pthread_mutex_lock(&ptr_ctx->lock);
            prec_rpc_udata* udat = udata;
            rpc_struct_prec_ctx* ctx = hashmap_get(&ptr_ctx->keys,udat->name);
            if(ctx){
                for(int i = 0; i < ctx->o_index; i++){
                    if(ctx->origins[i] == udat->origin){
                        ctx->origins[i] = NULL;

                        if(i == ctx->o_index - 1) ctx->o_index--;
                        else sc_queue_add_last(&ctx->empty_origins,i);
                    }
                }
                if(ctx->o_index == 0){
                    hashmap_remove(&ptr_ctx->keys,udat->name);
                    sc_queue_term(&ctx->empty_origins);
                    free(ctx->origins);
                    free(ctx);
                }
            }
            pthread_mutex_unlock(&ptr_ctx->lock);
        }
    }
}

struct prec_callbacks rpc_struct_default_prec_cbs = {
    .zero = rpc_struct_onzero_cb,
    .increment = rpc_struct_increment_cb,
    .decrement = rpc_struct_decrement_cb,
};

//=====================================================

void rpc_struct_free_internals(rpc_struct_t rpc_struct){
    if(rpc_struct){
        if(rpc_struct->manual_destructor) rpc_struct->manual_destructor(rpc_struct);
        pthread_mutex_lock(&rpc_struct->lock);

        const char* foreach_key = NULL;
        void* pos = NULL;
        hashmap_foreach_key_safe(foreach_key,&rpc_struct->map,pos){
            rpc_struct_remove(rpc_struct,(char*)foreach_key);
        }

        pthread_mutex_unlock(&rpc_struct->lock);
        hashmap_cleanup(&rpc_struct->map);
    }
}

static void rpc_struct_free_internal(rpc_struct_t rpc_struct){
    rpc_struct_free_internals(rpc_struct);
    free(rpc_struct);
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
    pthread_mutex_lock(&rpc_struct->lock);
    memcpy(rpc_struct->ID,ID,strlen(ID));
    pthread_mutex_unlock(&rpc_struct->lock);
}

int rpc_is_pointer(enum rpc_types type){ //return 1 if rpc_type is pointer, 0 if not
    int ret = 0;
    if(type == RPC_struct || type == RPC_string || type == RPC_sizedbuf || type == RPC_function || type == RPC_unknown) ret = 1;

    return ret;
}
rpc_struct_free_cb rpc_freefn_of(enum rpc_types type){
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
        pthread_mutex_lock(&rpc_struct->lock);
        rpc_type_t* element = hashmap_get(&rpc_struct->map,key);
        if(element){
            if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
                prec_rpc_udata udat = {
                    .name = key,
                    .origin = rpc_struct,
                };
                prec_decrement(prec_get(element->data_container.ptr_value),&udat);
            } else if(element->type == RPC_string) free(element->data_container.ptr_value);

            hashmap_remove(&rpc_struct->map,key);

            free(element);

            pthread_mutex_unlock(&rpc_struct->lock);
            return 0;
        }
        pthread_mutex_unlock(&rpc_struct->lock);
        return 1;
    }
    return 1;
}


struct rpc_struct_duplicate_info {
    enum rpc_types type;
    char* original_name;

    size_t duplicates_len;
    char** duplicates;
};

#ifdef RPC_SERIALISERS
//THIS CODE WAS HUGGELY REFACTORED VIA AI AND EDITED BY ME(Catmengi), IF YOU FIND BUG IN IT, REPORT IMMEDIATLY!
struct rpc_struct_duplicate_info* rpc_struct_found_duplicates(rpc_struct_t rpc_struct, size_t* len_output) {
    assert(rpc_struct);
    *len_output = 0;
    rpc_struct_manual_lock(rpc_struct);

    size_t length = rpc_struct_length(rpc_struct);
    char** keys = rpc_struct_keys(rpc_struct);
    if (!keys) {rpc_struct_manual_unlock(rpc_struct);return NULL;}

    // Создаём хеш-таблицу: ключ — указатель (void*), значение — struct rpc_struct_duplicate_info*
    HASHMAP(void,void) ptr_map;
    hashmap_init(&ptr_map, ptracker_hash_ptr,ptracker_ptr_cmp);

    // Динамический массив для результата
    size_t capacity = 16;
    struct rpc_struct_duplicate_info* duplicates = malloc(capacity * sizeof(*duplicates));
    assert(duplicates);
    size_t count = 0;

    void* pos = NULL;
    rpc_type_t* element = NULL;

    size_t i = 0;
    hashmap_foreach_data_safe(element,&rpc_struct->map,pos){
        if (rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown) {
            // Ищем ptr в ptr_map
            struct rpc_struct_duplicate_info* info = hashmap_get(&ptr_map, element->data_container.ptr_value);
            if (info == NULL) {
                // Новый объект — расширяем массив при необходимости
                if (count == capacity) {
                    capacity *= 2;
                    assert((duplicates = realloc(duplicates, capacity * sizeof(*duplicates))) != NULL);
                }
                // Инициализируем info
                info = &duplicates[count++];
                info->type = element->type;
                info->original_name = keys[i];
                info->duplicates_len = 0;
                info->duplicates = NULL;

                // Добавляем в хеш-таблицу
                hashmap_put(&ptr_map, element->data_container.ptr_value, info);
            } else {
                //adding keys to duplicates, since wine dont know ammount of duplicates before hand, just do realloc!
                assert((info->duplicates = realloc(info->duplicates, ++info->duplicates_len * sizeof(*info->duplicates))) != NULL);
                info->duplicates[info->duplicates_len - 1] = keys[i];
            }
        }
        i++;
    }

    free(keys);
    hashmap_cleanup(&ptr_map);
    if (count == 0) {
        free(duplicates);
        rpc_struct_manual_unlock(rpc_struct);
        return NULL;
    }

    *len_output = count;
    rpc_struct_manual_unlock(rpc_struct);
    return duplicates;
}
//==========================================================================

#define STRINGIFY(x) #x
json_t* rpc_struct_serialize(rpc_struct_t rpc_struct){
    pthread_mutex_lock(&rpc_struct->lock);
    rpc_struct_t dupless_struct = rpc_struct_copy(rpc_struct);

    size_t dups_len = 0;
    struct rpc_struct_duplicate_info* dups = rpc_struct_found_duplicates(rpc_struct,&dups_len);

    const char* dsr_foreach_key = NULL;
    rpc_type_t* dsr_el = NULL;
    void* dsr_pos = NULL;

    size_t skip_items = 0;
    hashmap_foreach_safe(dsr_foreach_key,dsr_el,&dupless_struct->map,dsr_pos){
        if(dsr_el->type == RPC_unknown){
            rpc_struct_remove(dupless_struct,(char*)dsr_foreach_key);
            skip_items++;
        }
    }
    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            rpc_struct_remove(dupless_struct,dups[i].duplicates[j]);
        }
    }

    json_t* root = json_object(); assert(root);
    json_object_set_new(root, "ID", json_string(rpc_struct->ID));
    json_object_set_new(root, "type", json_string(STRINGIFY(RPC_struct)));

    json_t* serialised = json_object();
    json_object_set_new(root,"serialised",serialised);

    void* main_pos = NULL;
    const char* main_foreach_key = NULL;
    rpc_type_t* el = NULL;

    hashmap_foreach_safe(main_foreach_key,el,&dupless_struct->map,main_pos){
        json_t* item = NULL;
        if(rpc_is_pointer(el->type) && el->type != RPC_string){
            switch(el->type){
                case RPC_struct:
                    item = rpc_struct_serialize(el->data_container.ptr_value);
                    break;
                case RPC_function:
                    item = rpc_function_serialize(el->data_container.ptr_value);
                    break;
                case RPC_sizedbuf:
                    item = rpc_sizedbuf_serialize(el->data_container.ptr_value);
                    break;
                default: break;
            }
        } else {
            switch(el->type){
                case RPC_number:{
                    json_int_t json_int = 0;
                    switch(el->length){
                        case sizeof(uint8_t):
                            json_int = *(uint8_t*)el->data_container.raw_value;
                            break;
                        case sizeof(uint16_t):
                            json_int = *(uint16_t*)el->data_container.raw_value;
                            break;
                        case sizeof(uint32_t):
                            json_int = *(uint32_t*)el->data_container.raw_value;
                            break;
                        case sizeof(uint64_t):
                            json_int = *(uint64_t*)el->data_container.raw_value;
                            break;
                    }
                    item = json_integer(json_int);
                }
                break;
            case RPC_real:{
                double json_double = 0;
                switch(el->length){
                    case sizeof(float):
                        json_double = *(float*)el->data_container.raw_value;
                        break;
                    case sizeof(double):
                        json_double = *(double*)el->data_container.raw_value;
                        break;
                }
                item = json_real(json_double);
                }
                break;
                    case RPC_string:
                        item = json_string(el->data_container.ptr_value);
                        break;
                default: break;
            }
        }
        json_object_set_new(serialised,main_foreach_key, item);
    }
    rpc_struct_free(dupless_struct);

    json_t* duplicates = json_object();
    json_object_set_new(root,"duplicates",duplicates);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            if(dups[i].duplicates[j])
                json_object_set_new(duplicates,dups[i].duplicates[j],json_string(dups[i].original_name));
        }
        free(dups[i].duplicates);
    }
    free(dups);
    pthread_mutex_unlock(&rpc_struct->lock);
    return root;
}
static void item_parse(json_t* item, rpc_struct_t rpc_struct, char* key){
    switch(json_typeof(item)){
        case JSON_INTEGER:
            rpc_struct_set(rpc_struct,key, (uint64_t)json_integer_value(item));
            break;

        case JSON_REAL:
            rpc_struct_set(rpc_struct, key, json_real_value(item));
            break;

        case JSON_STRING:
            rpc_struct_set(rpc_struct,key,(char*)json_string_value(item));
            break;

        case JSON_OBJECT:{ //using braces due to variable declaration!
            const char* item_type = json_string_value(json_object_get(item,"type"));

            if(strcmp(item_type, STRINGIFY(RPC_struct)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_struct_deserialize(item));
            } else if(strcmp(item_type, STRINGIFY(RPC_function)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_function_deserialize(item));
            } else if(strcmp(item_type, STRINGIFY(RPC_sizedbuf)) == 0){
                rpc_struct_set(rpc_struct, key, rpc_sizedbuf_deserialize(item));
            } else return;
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
rpc_struct_t rpc_struct_deserialize(json_t* json){
    if(json == NULL) return NULL;

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
            rpc_struct_set_internal(new,(char*)key,rpc_struct_get_internal(new,(char*)original));
        }
    }
    return new;

bad_exit:
    rpc_struct_free(new);
    return NULL;
}
#endif

rpc_struct_t rpc_struct_copy(rpc_struct_t original){
    rpc_struct_manual_lock(original);
    rpc_struct_t copy = rpc_struct_create();
    rpc_struct_id_set(copy, rpc_struct_id_get(original));

    void* pos = NULL;
    const char* foreach_key = NULL;
    rpc_type_t* element = NULL;
    hashmap_foreach_safe(foreach_key,element,&original->map,pos){
        rpc_type_t element_copy = *element;
        if(rpc_is_pointer(element_copy.type) && element_copy.type != RPC_string && element_copy.type != RPC_unknown){
            prec_rpc_udata udat = {
                .name = (char*)foreach_key,
                .origin = copy,
                .free = rpc_freefn_of(element_copy.type),
            };
            prec_increment(prec_get(element_copy.data_container.ptr_value),&udat);
        }
        rpc_struct_set_internal(copy,(char*)foreach_key,element_copy);
    }
    copy->copyof = original;
    rpc_struct_manual_unlock(original);
    return copy;
}

rpc_struct_t rpc_struct_deep_copy(rpc_struct_t original){
    rpc_struct_manual_lock(original);
    rpc_struct_t copy = rpc_struct_create();
    rpc_struct_id_set(copy, rpc_struct_id_get(original));

    HASHMAP(void,void) duptrack;
    hashmap_init(&duptrack,ptracker_hash_ptr,ptracker_ptr_cmp);

    void* pos = NULL;
    const char* foreach_key = NULL;
    rpc_type_t* element = NULL;
    hashmap_foreach_safe(foreach_key,element,&original->map,pos){
        rpc_type_t element_copy = *element;
        if(element_copy.type == RPC_string){
            void* tmp = malloc(element_copy.length); assert(tmp);
            memcpy(tmp,element_copy.data_container.ptr_value,element_copy.length);
            element_copy.data_container.ptr_value = tmp;
        } else if(rpc_is_pointer(element_copy.type) && element_copy.type != RPC_string && element_copy.type != RPC_unknown){

            switch(element_copy.type){
                case RPC_struct:{
                    void* is_copy = hashmap_get(&duptrack,element_copy.data_container.ptr_value);
                    if(is_copy == NULL){
                        void* old_ptr = element_copy.data_container.ptr_value;
                        element_copy.data_container.ptr_value = rpc_struct_deep_copy(element_copy.data_container.ptr_value);
                        hashmap_put(&duptrack,old_ptr,element_copy.data_container.ptr_value);
                    } else element_copy.data_container.ptr_value = is_copy;
                }
                break;

                case RPC_sizedbuf:{
                    void* is_copy = hashmap_get(&duptrack,element_copy.data_container.ptr_value);
                    if(is_copy == NULL){
                        void* old_ptr = element_copy.data_container.ptr_value;
                        element_copy.data_container.ptr_value = rpc_sizedbuf_copy(element_copy.data_container.ptr_value);
                        hashmap_put(&duptrack,old_ptr,element_copy.data_container.ptr_value);
                    } else element_copy.data_container.ptr_value = is_copy;
                }
                break;

                case RPC_function:{
                    void* is_copy = hashmap_get(&duptrack,element_copy.data_container.ptr_value);
                    if(is_copy == NULL){
                        void* old_ptr = element_copy.data_container.ptr_value;
                        element_copy.data_container.ptr_value = rpc_function_copy(element_copy.data_container.ptr_value);
                        hashmap_put(&duptrack,old_ptr,element_copy.data_container.ptr_value);
                    } else element_copy.data_container.ptr_value = is_copy;
                }
                break;

                default: break;
            }

            prec_rpc_udata udat = {
                .name = (char*)foreach_key,
                .origin = copy,
                .free = rpc_freefn_of(element_copy.type),
            };
            prec_increment(prec_get(element_copy.data_container.ptr_value),&udat);
        }

        rpc_struct_set_internal(copy,(char*)foreach_key,element_copy);
    }
    hashmap_cleanup(&duptrack);
    copy->copyof = original;
    rpc_struct_manual_unlock(original);
    return copy;
}

rpc_struct_t rpc_struct_whose_copy(rpc_struct_t rpc_struct){
    pthread_mutex_lock(&rpc_struct->lock);
    rpc_struct_t parent = rpc_struct->copyof;
    if(rpc_struct->copyof != NULL){
        prec_t parent_prec = prec_get(rpc_struct->copyof);
        if(parent_prec == NULL){ //object no longer exist or parent rpc_struct is NOT TRACKER EVERYWHERE ELSE?
            rpc_struct->copyof = NULL;
            parent = NULL;
        }
    }
    pthread_mutex_unlock(&rpc_struct->lock);
    return parent;
}

size_t rpc_struct_length(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return hashmap_size(&rpc_struct->map);
}
char** rpc_struct_keys(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    rpc_struct_manual_lock(rpc_struct);
    char** keys = malloc(sizeof(*keys) * hashmap_size(&rpc_struct->map)); assert(keys);

    size_t i = 0;
    void* pos = NULL;
    const char* foreach_key = NULL;
    hashmap_foreach_key_safe(foreach_key,&rpc_struct->map,pos){
        keys[i++] = (char*)foreach_key;
    }

    rpc_struct_manual_unlock(rpc_struct);

    return keys;
}

enum rpc_types rpc_struct_typeof(rpc_struct_t rpc_struct, char* key){
    assert(rpc_struct);
    rpc_struct_manual_lock(rpc_struct);
    rpc_type_t element = rpc_struct_get_internal(rpc_struct,key);
    rpc_struct_manual_unlock(rpc_struct);

    return element.type;
}

int rpc_struct_exists(rpc_struct_t rpc_struct, char* key){
    rpc_struct_manual_lock(rpc_struct);
    int ret = (rpc_struct_get_internal(rpc_struct,key).type == RPC_none ? 0 : 1);
    rpc_struct_manual_unlock(rpc_struct);

    return ret;
}

int rpc_struct_set_internal(rpc_struct_t rpc_struct, char* key, rpc_type_t element){
    rpc_struct_manual_lock(rpc_struct);
    if(rpc_is_pointer(element.type) && element.data_container.ptr_value != NULL || !rpc_is_pointer(element.type)){
        if(hashmap_get(&rpc_struct->map,key) == NULL){
            if(element.type == RPC_string){
                element.data_container.ptr_value = strdup(element.data_container.ptr_value); assert(element.data_container.ptr_value);
                element.length = strlen(element.data_container.ptr_value) + 1;
            }
            if(rpc_is_pointer(element.type) && element.type != RPC_string && element.type != RPC_unknown){
                prec_t prec = prec_get(element.data_container.ptr_value);
                if(prec == NULL) prec = prec_new(element.data_container.ptr_value,rpc_struct_default_prec_cbs);

                prec_rpc_udata udat = {
                    .name = key,
                    .origin = rpc_struct,
                    .free = rpc_freefn_of(element.type),
                };
                prec_increment(prec,&udat);
            }

            assert(hashmap_put(&rpc_struct->map,key,copy(&element)) == 0);
            rpc_struct_manual_unlock(rpc_struct);
            return 0;
        }
    }
    rpc_struct_manual_unlock(rpc_struct);
    return 1;
}

rpc_type_t rpc_struct_get_internal(rpc_struct_t rpc_struct, char* key){
    rpc_type_t cont = {0};
    rpc_type_t* tmp_cont = NULL;
    rpc_struct_manual_lock(rpc_struct);
    if(rpc_struct && key){
        tmp_cont = hashmap_get(&rpc_struct->map,key);
        if(tmp_cont) cont = *(rpc_type_t*)tmp_cont;
    }
    rpc_struct_manual_unlock(rpc_struct);

    return cont;
}

void rpc_struct_decrement_refcount(void* ptr){
    prec_decrement(prec_get(ptr),NULL);
}

uint64_t rpc_struct_hash(rpc_struct_t rpc_struct){
    uint64_t hash = 0;
    rpc_struct_manual_lock(rpc_struct);
    void* pos = NULL;
    rpc_type_t* element = NULL;
    hashmap_foreach_data_safe(element,&rpc_struct->map,pos){
        uint64_t new_hash = hash;
        if(rpc_is_pointer(element->type)){
            switch(element->type){
                case RPC_struct:
                    new_hash += rpc_struct_hash(element->data_container.ptr_value);
                    break;
                case RPC_sizedbuf:
                    new_hash += rpc_sizedbuf_hash(element->data_container.ptr_value);
                    break;
                case RPC_string:
                    new_hash += murmur((uint8_t*)element->data_container.ptr_value,element->length);
                    break;
                default: new_hash += murmur(element->data_container.ptr_value,sizeof(element->data_container.ptr_value)); break;
            }
        } else new_hash += murmur((uint8_t*)element->data_container.raw_value,element->length);

        hash = murmur((uint8_t*)&new_hash,sizeof(new_hash));
    }

    rpc_struct_manual_unlock(rpc_struct);
    return hash;
}

void rpc_struct_manual_lock(rpc_struct_t rpc_struct){
    if(rpc_struct){
        pthread_mutex_lock(&rpc_struct->lock);
    }
}
void rpc_struct_manual_unlock(rpc_struct_t rpc_struct){
    if(rpc_struct){
        pthread_mutex_unlock(&rpc_struct->lock);
    }
}

int rpc_struct_is_refcounted(void* ptr){
    return prec_get(ptr) == NULL ? 0 : 1;
}
