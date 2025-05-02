#include <rpc_struct.h>
#include <rpc_sizedbuf.h>
#include <hashtable.h>
#include <sc_queue.h>

#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

struct _rpc_struct{
    hashtable* ht;

    hashtable** refcount_copys;
    atomic_size_t refcount_copys_len;
    atomic_size_t refcount_copys_index;
    struct sc_queue_int REF_freed;

    hashtable* anti_double_free;  //used to not free elements that have different keys but same data
    atomic_size_t* ADF_refcount;

    rpc_struct_t* copys;
    atomic_size_t copys_len;
    atomic_size_t copys_index;
    struct sc_queue_int CPY_freed;

    atomic_bool run_GC;

    rpc_struct_t parent;
};

rpc_struct_t rpc_struct_create(void){
    rpc_struct_t rpc_struct = (rpc_struct_t)malloc(sizeof(*rpc_struct));
    assert(rpc_struct);

    rpc_struct->ht = hashtable_create();
    rpc_struct->anti_double_free = hashtable_create();

    rpc_struct->refcount_copys_len = 4;
    rpc_struct->refcount_copys_index = 0;
    rpc_struct->refcount_copys = calloc(rpc_struct->refcount_copys_len,sizeof(*rpc_struct->refcount_copys)); assert(rpc_struct->refcount_copys);
    rpc_struct->refcount_copys[rpc_struct->refcount_copys_index++] = rpc_struct->ht;

    rpc_struct->ADF_refcount = malloc(sizeof(*rpc_struct->ADF_refcount)); assert(rpc_struct->ADF_refcount);
    *rpc_struct->ADF_refcount = 1;

    rpc_struct->copys_len = 0;
    rpc_struct->copys_index = 0;
    rpc_struct->copys = NULL;

    rpc_struct->run_GC = 0;
    rpc_struct->parent = NULL;

    sc_queue_init(&rpc_struct->CPY_freed);
    sc_queue_init(&rpc_struct->REF_freed);

    return rpc_struct;
}

//used to implement OPAQUE
void* rpc_struct_HT(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return rpc_struct->ht;
}
void* rpc_struct_ADF(rpc_struct_t rpc_struct){
    assert(rpc_struct);
    return rpc_struct->anti_double_free;
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

void rpc_struct_cleanup(rpc_struct_t rpc_struct){
    if(rpc_struct->anti_double_free->size > 0){
        for(size_t i = 0; i < rpc_struct->anti_double_free->capacity; i++){
            if(rpc_struct->anti_double_free->body[i].key != NULL && rpc_struct->anti_double_free->body[i].key != (char*)0xDEAD && rpc_struct->anti_double_free->body[i].value != NULL){
                size_t refcount = 0; //If it stays zero we remove this entry from anti_double_free
                struct rpc_container_element* GC_copy = rpc_struct->anti_double_free->body[i].value;

                for(size_t r = 0; r < rpc_struct->refcount_copys_index; r++){
                    if(rpc_struct->refcount_copys[r]){
                        for(size_t j = 0; j < rpc_struct->refcount_copys[r]->capacity; j++){
                            if(rpc_struct->refcount_copys[r]->body[j].key != (char*)0xDEAD &&
                                rpc_struct->refcount_copys[r]->body[j].key != NULL && rpc_struct->refcount_copys[r]->body[j].value != NULL){

                                struct rpc_container_element* check_element = rpc_struct->refcount_copys[r]->body[j].value;
                                if(check_element->data == GC_copy->data) refcount++; //belive me, this is how it should be done
                            }
                        }
                    }
                }

                if(refcount == 0){
                    rpc_container_free(GC_copy);
                    free(GC_copy);

                    char* key_cpy = rpc_struct->anti_double_free->body[i].key;
                    hashtable_remove(rpc_struct->anti_double_free,key_cpy);
                    free(key_cpy);
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

    if(--(*rpc_struct->ADF_refcount) == 0) {free(rpc_struct->ADF_refcount);hashtable_destroy(rpc_struct->anti_double_free);}
    hashtable_destroy(rpc_struct->ht);

    for(size_t i = 0;  i < rpc_struct->copys_index; i++){
        rpc_struct_t copy = rpc_struct->copys[i];
        if(copy){
            for(size_t j = 0; j < copy->refcount_copys_index; j++){
                if(copy->refcount_copys[j] == rpc_struct->ht){
                    assert(j != 0); //sanity check
                    copy->refcount_copys[j] = NULL;
                    sc_queue_add_last(&copy->REF_freed,j);
                }
            }
        }
    }
    if(rpc_struct->parent){
        rpc_struct_t current = rpc_struct->parent;
        while(current){
            for(size_t j = 0; j < current->refcount_copys_index; j++){
                if(current->refcount_copys[j] == rpc_struct->ht){
                    current->refcount_copys[j] = NULL;
                    sc_queue_add_last(&current->REF_freed,j);
                }
            }
            for(size_t i = 0; i < current->copys_index; i++){
                if(current->copys[i] == rpc_struct){
                    current->copys[i] = NULL;
                    sc_queue_add_last(&current->CPY_freed,i);
                }
            }
            current = current->parent;
        }
    } else {
        for(size_t i = 0; i < rpc_struct->copys_index; i++){
            rpc_struct_t copy = rpc_struct->copys[i];
            if(copy) copy->parent = NULL;
        }
    }
    sc_queue_term(&rpc_struct->REF_freed);
    sc_queue_term(&rpc_struct->CPY_freed);

    free(rpc_struct->copys);
    free(rpc_struct->refcount_copys);
    free(rpc_struct);
}

int rpc_struct_unlink(rpc_struct_t rpc_struct, char* key){
    if(rpc_struct == NULL || key == NULL) return 1;

    struct rpc_container_element* element = NULL;
    if((element = hashtable_get(rpc_struct->ht,key)) != NULL){
        if(rpc_is_pointer(element->type)){
            char* free_key = rpc_struct->ht->body[hashtable_find_slot(rpc_struct->ht,key)].key;
            hashtable_remove(rpc_struct->ht,key);

            free(element); //We should free container BUT NOT internals!
            free(free_key); //since key is strdup() ed we should free it
            return 0;
        }
    }
    return 1;
}

int rpc_struct_remove(rpc_struct_t rpc_struct, char* key){
    if(rpc_struct == NULL || key == NULL) return 1;

    struct rpc_container_element* element = NULL;
    if((element = hashtable_get(rpc_struct->ht,key)) != NULL){
        char* free_key = rpc_struct->ht->body[hashtable_find_slot(rpc_struct->ht,key)].key;
        if(element->data != rpc_struct){ //WHY DID YOU DONE THIS?!?!
            hashtable_remove(rpc_struct->ht,key);

            if(rpc_is_pointer(element->type) && element->type != RPC_string){ //ignore strings because they are unique (since strdup'd)
                free(element);
                free(free_key); //since key is strdup() ed we should free it
                rpc_struct->run_GC = 1; //run GC on next rpc_struct_set
            } else {rpc_container_free(element); free(element); free(free_key);}
            return 0;
        }
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
    *len_output = rpc_struct->anti_double_free->size;
    if(*len_output > 0){
        int DI = 0;
        struct rpc_struct_duplicate_info* duplicate_info = calloc(*len_output,sizeof(*duplicate_info)); assert(duplicate_info);
        for(size_t i = 0; i < rpc_struct->anti_double_free->capacity; i++){
            size_t start_from = 0;
            if(rpc_struct->anti_double_free->body[i].key != NULL && rpc_struct->anti_double_free->body[i].key != (char*)0xDEAD && rpc_struct->anti_double_free->body[i].value != NULL){
                struct rpc_container_element* GC_copy = rpc_struct->anti_double_free->body[i].value;
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
                        assert(duplicate_info[DI].original); //Shit cannot happen there!
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
    uint64_t u64_parse_len = *(uint64_t*)buf;

    buf += sizeof(uint64_t);
    struct rpc_serialise_element serialise;

    for(uint64_t i = 0; i < u64_parse_len; i++){
        serialise.key = buf; buf += strlen(serialise.key) + 1;
        serialise.type = *buf; buf++;
        serialise.buflen = *(uint64_t*)buf; buf += sizeof(uint64_t);
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

            char NOdoublefree[sizeof(void*) * 2];
            sprintf(NOdoublefree,"%p",unserialised);

            assert(hashtable_get(new->anti_double_free,NOdoublefree) == NULL); //asserting if element unique!

            struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);
            struct rpc_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);

            element->data = unserialised;
            element->type = serialise.type;
            element->length = 0;
            *GC_copy = *element;

            hashtable_set(new->anti_double_free,strdup(NOdoublefree),GC_copy);
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

            hashtable_set(new->ht,strdup(serialise.key),DUP_element);
        }
    }
    return new;
}

#define copy(input) ({void* __out = malloc(sizeof(*input)); assert(__out); memcpy(__out,input,sizeof(*input)); (__out);})

rpc_struct_t rpc_struct_copy(rpc_struct_t original){
    rpc_struct_t copy = rpc_struct_create();
    copy->parent = original;

    copy->ht->capacity = original->ht->capacity;
    copy->ht->size = original->ht->size;
    assert((copy->ht->body = realloc(copy->ht->body,copy->ht->capacity * sizeof(*copy->ht->body))));
    memcpy(copy->ht->body,original->ht->body,sizeof(*copy->ht->body) * copy->ht->capacity);

    for(size_t i = 0; i <copy->ht->capacity; i++){
        if(copy->ht->body[i].key != NULL && copy->ht->body[i].key != (void*)0xDEAD){
            copy->ht->body[i].key = strdup(copy->ht->body[i].key); //recoping keys because it WILL cause double-free if we not done this
            copy->ht->body[i].value = copy((struct rpc_container_element*)copy->ht->body[i].value);

            struct rpc_container_element* element = copy->ht->body[i].value;
            if(!rpc_is_pointer(element->type) || element->type == RPC_string){
                void* tmp = malloc(element->length); assert(tmp);
                memcpy(tmp,element->data,element->length);
                element->data = tmp;
            }
        }
    }

    hashtable_destroy(copy->anti_double_free);
    copy->anti_double_free = original->anti_double_free;

    free(copy->ADF_refcount);
    copy->ADF_refcount = original->ADF_refcount;
    (*copy->ADF_refcount)++;

    rpc_struct_t current = original;
    while(current){
        size_t index = 0;
        if(sc_queue_size(&current->REF_freed) == 0){
            if(current->refcount_copys_len - 1 == current->refcount_copys_index){
                current->refcount_copys_len += (current->refcount_copys_len / 2 == 0 ? 1 : current->refcount_copys_len / 2 );
                assert((current->refcount_copys = realloc(current->refcount_copys, current->refcount_copys_len * sizeof(*current->refcount_copys))));
            }
            index = current->refcount_copys_index++;
        } else index = sc_queue_del_first(&current->REF_freed);

        current->refcount_copys[index] = copy->ht;

        if(copy->refcount_copys_len - 1 == copy->copys_index){
            copy->refcount_copys_len += (copy->refcount_copys_len / 2 == 0 ? 1 : copy->refcount_copys_len / 2 );
            assert((copy->refcount_copys = realloc(copy->refcount_copys, copy->refcount_copys_len * sizeof(*copy->refcount_copys))));
        }
        copy->refcount_copys[copy->refcount_copys_index++] = current->ht;

        if(current->copys == NULL){
            current->copys_len = 4;
            current->copys_index = 0;
            current->copys = malloc(current->copys_len * sizeof(*current->copys)); assert(current->copys);
        }
        size_t cpy_index = 0;
        if(sc_queue_size(&current->CPY_freed) == 0){
            if(current->copys_len - 1 == current->copys_index){
                current->copys_len += (current->copys_len / 2 == 0 ? 1 : current->copys_len / 2 );
                assert((current->copys = realloc(current->copys,current->copys_len * sizeof(*current->copys))));
            }
            cpy_index = current->copys_index++;
        } else cpy_index = sc_queue_del_first(&current->CPY_freed);
        current->copys[cpy_index] = copy;
        current = current->parent;
    }
    return copy;
}


//REMOVE WHEN DONE!
int main(){
    rpc_struct_t rpc_struct = rpc_struct_create();

    uint64_t input = 12345678;
    rpc_struct_t DFC = rpc_struct_create();
    rpc_struct_set(rpc_struct,"check_int",input);
    rpc_struct_set(rpc_struct,"check_string",(char*)"test 1234567890000000000");
    char* K = malloc(10000);
    for(int i = 0; i < 5000; i++){
        sprintf(K,"%d",i);
        rpc_struct_set(rpc_struct,K,DFC);
    }
    rpc_struct_t DFC2 = rpc_struct_create();
    for(int i = 0; i < 5000; i++){
        sprintf(K,"TI%d",i);
        rpc_struct_set(rpc_struct,K,DFC2);
    }
    rpc_struct_set(rpc_struct,"szbuf",rpc_sizedbuf_create("hello!",sizeof("hello!")));


    uint64_t output;
    assert(rpc_struct_get(rpc_struct,"check_int",output) == 0);
    assert(output == input);
    assert(rpc_struct_unlink(rpc_struct,"check_int") != 0); //checking that it works properly on int!
    assert(rpc_struct_remove(rpc_struct,"check_int") == 0);
    assert(rpc_struct_set(DFC,"1234",(char*)"some data that should be in this very struct!") == 0);
    assert(rpc_struct_set(rpc_struct,"I1234",(char*)"1234") == 0);

    char* str = NULL;
    rpc_struct_get(rpc_struct,"check_string",str);
    rpc_struct_unlink(rpc_struct,"check_string");
    free(str);

    size_t buflen = 0;
    char* buf = rpc_struct_serialise(rpc_struct,&buflen);
    uint64_t print = *(uint64_t*)buf;
    printf("%lu\n",print);
    FILE* wr = fopen("debug_test_output","wra");
    fwrite(buf,buflen,1,wr);
    fclose(wr);

    rpc_struct_t unser = rpc_struct_unserialise(buf);
    rpc_struct_t copy = rpc_struct_copy(rpc_struct);

    rpc_struct_t unser_C1;
    rpc_struct_get(unser,"0",unser_C1);
    rpc_struct_t unser_C2;
    rpc_struct_get(unser,"TI0",unser_C2);

    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"%d",i);
        rpc_struct_get(unser,K,C);
        assert(C == unser_C1);

        char* S;
        assert(rpc_struct_get(C,"1234",S) == 0);
        assert(strcmp(S,"some data that should be in this very struct!") == 0);
    }
    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"TI%d",i);
        rpc_struct_get(unser,K,C);
        assert(C == unser_C2);
    }

    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"%d",i);
        rpc_struct_get(copy,K,C);

        char* S;
        assert(rpc_struct_get(C,"1234",S) == 0);
        assert(strcmp(S,"some data that should be in this very struct!") == 0);
        rpc_struct_remove(copy,K);
    }
    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"TI%d",i);
        rpc_struct_get(copy,K,C);
        rpc_struct_remove(copy,K);
    }

    free(buf);

    // *(int*)1 = 0;
    size_t UN = 0;
    free(rpc_struct_serialise(copy,&UN));
    rpc_struct_free(rpc_struct_copy(rpc_struct));
    rpc_struct_free(unser);

    free(rpc_struct_serialise(copy,&UN));
    rpc_struct_t CC = rpc_struct_copy(copy);

    rpc_sizedbuf_t szbuf = NULL;
    rpc_struct_get(CC,"szbuf",szbuf);
    printf("szbuf check! %s\n",rpc_sizedbuf_getbuf(szbuf,&UN));

    rpc_struct_free(copy);
    rpc_struct_free(rpc_struct);
    rpc_struct_free(CC);


    free(K);

}
//=================
