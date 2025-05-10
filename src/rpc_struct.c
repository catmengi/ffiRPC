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
#include "../include/sc_queue.h"

#include <stdint.h>
#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

static hashtable* ADF_ht = NULL;

__attribute__((constructor))
void rpc_struct_ADF_init(void){
    if(ADF_ht == NULL){
        ADF_ht = hashtable_create();
    }
}

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
    atomic_bool run_GC;
};

rpc_struct_t rpc_struct_create(void){
    rpc_struct_t rpc_struct = (rpc_struct_t)malloc(sizeof(*rpc_struct));
    assert(rpc_struct);

    rpc_struct->ht = hashtable_create();
    rpc_struct->run_GC = 0;

    return rpc_struct;
}

hashtable* rpc_struct_HT(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return rpc_struct->ht;
}
//========================


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

size_t rpc_struct_refcountof(rpc_struct_t rpc_struct, void* cmp){
//    size_t refcount = 0;
//    for(size_t r = 0; r < rpc_struct->refcounts_index; r++){
//        if(rpc_struct->refcounts[r]){
//            for(size_t j = 0; j < rpc_struct->refcounts[r]->capacity; j++){
//                if(rpc_struct->refcounts[r]->body[j].key != (char*)0xDEAD &&
//                    rpc_struct->refcounts[r]->body[j].key != NULL && rpc_struct->refcounts[r]->body[j].value != NULL){
//
//                    struct rpc_container_element* check_element = rpc_struct->refcounts[r]->body[j].value;
//                    if(check_element->data == cmp) refcount++; //belive me, this is how it should be done
//                }
//            }
//        }
//    }
    char* NOdoublefree = malloc(sizeof(void*) * 4);
    sprintf(NOdoublefree,"%p",cmp);
    struct rpc_container_element* GC_copy = hashtable_get(ADF_ht,NOdoublefree);
    free(NOdoublefree);

    return (GC_copy == NULL ? assert(GC_copy),0 : GC_copy->refcount);
}

void rpc_struct_cleanup(rpc_struct_t rpc_struct){
    if(ADF_ht->size > 0){
        for(size_t i = 0; i < ADF_ht->capacity; i++){
            if(ADF_ht->body[i].key != NULL && ADF_ht->body[i].key != (char*)0xDEAD && ADF_ht->body[i].value != NULL){
                struct rpc_container_element* GC_copy = ADF_ht->body[i].value;

                if(rpc_struct_refcountof(rpc_struct,GC_copy->data) == 0){
                    char* key_cpy = ADF_ht->body[i].key;
                    hashtable_remove(ADF_ht,key_cpy);
                    free(key_cpy);

                    rpc_container_free(GC_copy);
                    free(GC_copy);
                }
            }
        }
    }
    rpc_struct->run_GC = 0; //we cleaned up, no need to run it now
}

void rpc_struct_free(rpc_struct_t rpc_struct){
    if(rpc_struct == NULL) return;

    for(size_t i = 0; i < rpc_struct->ht->capacity; i++){
        if(rpc_struct->ht->body[i].key != (char*)0xDEAD && rpc_struct->ht->body[i].key != NULL && rpc_struct->ht->body[i].value != NULL){
            rpc_struct_remove(rpc_struct,rpc_struct->ht->body[i].key);
         }
    }
    rpc_struct_cleanup(rpc_struct); //need to be sure we removed all elements

    hashtable_destroy(rpc_struct->ht);

    free(rpc_struct);
}

int rpc_struct_unlink(rpc_struct_t rpc_struct, void* ptr){
    if(rpc_struct == NULL || ptr == NULL) return 1;

        char* NOdoublefree = malloc(sizeof(void*) * 4);
        sprintf(NOdoublefree,"%p",ptr);
        struct rpc_container_element* GC_copy = hashtable_get(ADF_ht,NOdoublefree);
        if(GC_copy){
            if(rpc_is_pointer(GC_copy->type) && GC_copy->type != RPC_string && GC_copy->type != RPC_unknown){
                char* ADF_free_key = ADF_ht->body[hashtable_find_slot(ADF_ht,NOdoublefree)].key;
                free(GC_copy);
                hashtable_remove(ADF_ht,NOdoublefree); //removing this element from rpc_struct's garbage collector/reference counter/rpc_struct_cleanup()
                free(ADF_free_key);
                return 0;
            }
        }
        free(NOdoublefree);
    return 1;
}

int rpc_struct_remove(rpc_struct_t rpc_struct, char* key){
    if(rpc_struct == NULL || key == NULL) return 1;

    struct rpc_container_element* element = NULL;
    if((element = hashtable_get(rpc_struct->ht,key)) != NULL){
        char* free_key = rpc_struct->ht->body[hashtable_find_slot(rpc_struct->ht,key)].key;
        hashtable_remove(rpc_struct->ht,key);

        if(rpc_is_pointer(element->type) && element->type != RPC_string){ //ignore strings because they are unique (since strdup'd)
            char NOdoublefree[sizeof(void*) * 4];
            sprintf(NOdoublefree,"%p",element->data);
            struct rpc_container_element* GC_copy = hashtable_get(ADF_ht,NOdoublefree);
            if(GC_copy) GC_copy->refcount--;

            free(element);
            free(free_key); //since key is strdup() ed we should free it
            rpc_struct->run_GC = 1; //run GC on next rpc_struct_set
        } else {rpc_container_free(element); free(element); free(free_key);}
        return 0;
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
    struct rpc_container_element* original; //may not be original, but it was found first at least

    size_t duplicates_len;
    char** duplicates;
};

struct rpc_struct_duplicate_info* rpc_struct_found_duplicates(rpc_struct_t rpc_struct, size_t* len_output){
    rpc_struct_cleanup(rpc_struct);
    *len_output = ADF_ht->size;
    if(*len_output > 0){
        int DI = 0;
        struct rpc_struct_duplicate_info* duplicate_info = calloc(*len_output,sizeof(*duplicate_info)); assert(duplicate_info);
        for(size_t i = 0; i < ADF_ht->capacity; i++){
            size_t start_from = 0;
            if(ADF_ht->body[i].key != NULL && ADF_ht->body[i].key != (char*)0xDEAD && ADF_ht->body[i].value != NULL){
                struct rpc_container_element* GC_copy = ADF_ht->body[i].value;
                duplicate_info[DI].type = GC_copy->type;

                for(size_t j = 0; j < rpc_struct->ht->capacity; j++){
                    if(rpc_struct->ht->body[j].key != (char*)0xDEAD && rpc_struct->ht->body[j].key != NULL && rpc_struct->ht->body[j].value != NULL){
                        struct rpc_container_element* check_element = rpc_struct->ht->body[j].value;
                        if(duplicate_info[DI].original == NULL){
                            if(check_element->data == GC_copy->data){
                                duplicate_info[DI].original = check_element;
                                duplicate_info[DI].original_name = rpc_struct->ht->body[j].key;
                                start_from = j;
                            }
                        } else if(duplicate_info[DI].original != check_element && duplicate_info[DI].original->data == check_element->data) duplicate_info[DI].duplicates_len++;
                    }
                }
                size_t dupsI = 0;
                for(size_t j = start_from; j < rpc_struct->ht->capacity; j++){
                    if(rpc_struct->ht->body[j].key != (char*)0xDEAD && rpc_struct->ht->body[j].key != NULL && rpc_struct->ht->body[j].value != NULL){
                        if(duplicate_info[DI].original){
                            struct rpc_container_element* check_element = rpc_struct->ht->body[j].value;
                            if(duplicate_info[DI].duplicates == NULL){
                                duplicate_info[DI].duplicates = calloc(duplicate_info[DI].duplicates_len,sizeof(*duplicate_info[DI].duplicates));
                                assert(duplicate_info[DI].duplicates);
                            }

                            if(duplicate_info[DI].original != check_element && duplicate_info[DI].original->data == check_element->data){
                                assert(dupsI != duplicate_info[DI].duplicates_len); //No no no mister pointer you will not go to your SIGSEGV you will go in this ebaniy assert blyat
                                duplicate_info[DI].duplicates[dupsI] = rpc_struct->ht->body[j].key;
                                dupsI++;
                            }
                        }
                    }
                }
                DI++;
            }
        }
        return duplicate_info;
    } else return NULL;
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
    struct rpc_serialise_element* pre_serialise = calloc(serialise_elements_len,sizeof(*pre_serialise)); assert(pre_serialise);
    for(size_t i = 0; i < dupless_ht->capacity; i++){
        if(dupless_ht->body[i].key != (char*)0xDEAD && dupless_ht->body[i].key != NULL && dupless_ht->body[i].value != NULL){
            struct rpc_container_element* element = dupless_ht->body[i].value;
            pre_serialise[PS_index].type = element->type;
            pre_serialise[PS_index].key = dupless_ht->body[i].key;
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
    }
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
    size_t final_buflen = sizeof(uint64_t);
    for(size_t i = 0; i < serialise_elements_len; i++){
        if(pre_serialise[i].key == NULL) break;

        final_buflen += strlen(pre_serialise[i].key) + 1; //key
        final_buflen += 1; //type
        final_buflen += sizeof(uint64_t); //length of payload length
        final_buflen += pre_serialise[i].buflen; //payload length
        real_len++;
    }

    char* buf = malloc(final_buflen); assert(buf);
    char* write_buf = buf;

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

    rpc_struct_t new = rpc_struct_create();

    uint64_t u64_parse_len = 0;
    memcpy(&u64_parse_len,buf,sizeof(uint64_t)); buf += sizeof(uint64_t);

    struct rpc_serialise_element serialise;

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

            char* NOdoublefree = malloc(sizeof(void*) * 4);
            sprintf(NOdoublefree,"%p",unserialised);

            assert(hashtable_get(ADF_ht,NOdoublefree) == NULL); //asserting if element unique!

            struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);
            struct rpc_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);

            element->data = unserialised;
            element->type = serialise.type;
            element->length = 0;
            *GC_copy = *element;

            hashtable_set(ADF_ht,strdup(NOdoublefree),GC_copy);
            hashtable_set(new->ht,strdup(serialise.key),element);
            free(NOdoublefree);
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
                char* NOdoublefree = malloc(sizeof(void*) * 4);
                sprintf(NOdoublefree,"%p",element->data);
                struct rpc_container_element* GC_copy = hashtable_get(ADF_ht,NOdoublefree);
                free(NOdoublefree);

                if(GC_copy) GC_copy->refcount += (GC_copy->refcount / GC_copy->copy_count++);
            }
        }
    }
    return copy;
}

size_t rpc_struct_get_runGC(rpc_struct_t rpc_struct){
    return rpc_struct->run_GC;
}

size_t rpc_struct_length(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return rpc_struct->ht->size;
}
char** rpc_struct_getkeys(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    char** keys = malloc(sizeof(char*) * rpc_struct->ht->size); assert(keys);

    size_t j = 0;
    for(size_t i = 0; i < rpc_struct->ht->capacity; i++)
        if(rpc_struct->ht->body[i].key != NULL && rpc_struct->ht->body[i].key != (void*)0xDEAD)
            keys[j++] = rpc_struct->ht->body[i].key;
            
    return keys;
}

enum rpc_types rpc_struct_typeof(rpc_struct_t rpc_struct, char* key){
    assert(rpc_struct);
    struct rpc_container_element* element = hashtable_get(rpc_struct->ht,key);
    return (element == NULL ? 0 : element->type);
}

int rpc_struct_set_internal(rpc_struct_t rpc_struct, char* key, struct rpc_container_element* element){
    if(element->data == NULL) {free(element); return 1;}
    if(rpc_struct_get_runGC(rpc_struct)) rpc_struct_cleanup(rpc_struct);
        if(hashtable_get(rpc_struct->ht,key) == NULL){
            if(element->type == RPC_string){
                element->data = strdup(element->data); assert(element->data);
                element->length = strlen(element->data) + 1;
            }
            if(rpc_is_pointer(element->type) && element->type != RPC_string && element->type != RPC_unknown){
                char* NOdoublefree = malloc(sizeof(void*) * 4);
                sprintf(NOdoublefree,"%p",element->data);
                struct rpc_container_element* GC_copy = NULL;
                if((GC_copy = hashtable_get(ADF_ht,NOdoublefree)) == NULL){
                    GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);

                    GC_copy->data = element->data;
                    GC_copy->length = element->length;
                    GC_copy->type = element->type;
                    GC_copy->refcount = 1;
                    GC_copy->copy_count = 1;
                    hashtable_set(ADF_ht,strdup(NOdoublefree),GC_copy);
                } else GC_copy->refcount++;
                free(NOdoublefree);
            }
            hashtable_set(rpc_struct->ht,strdup(key),element);
            return 0;
        }
        return 1;
}

uint64_t rpc_struct_hash(rpc_struct_t rpc_struct){
    uint64_t hash = 0;
    char** keys = rpc_struct_getkeys(rpc_struct);
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
