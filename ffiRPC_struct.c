#include "ffiRPC_struct.h"
#include "hashtable.c/hashtable.h"

#include <stdint.h>
#include <assert.h>

struct _ffiRPC_struct{
    hashtable* ht;
    hashtable* anti_double_free;  //used to not free elements that have different keys but same data

    atomic_size_t size;
    atomic_bool run_GC;
};

ffiRPC_struct_t ffiRPC_struct_create(void){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->anti_double_free = hashtable_create();

    ffiRPC_struct->size = 0;
    ffiRPC_struct->run_GC = 0;

    return ffiRPC_struct;
}

//used to implement OPAQUE
void* ffiRPC_struct_HT(ffiRPC_struct_t ffiRPC_struct){
    assert(ffiRPC_struct);
    return ffiRPC_struct->ht;
}
void* ffiRPC_struct_ADF(ffiRPC_struct_t ffiRPC_struct){
    assert(ffiRPC_struct);
    return ffiRPC_struct->anti_double_free;
}
//========================


int ffiRPC_is_pointer(enum ffiRPC_types type){ //return 1 if ffiRPC_type is pointer, 0 if not
    int ret = 0;
    if(type == FFIRPC_struct || type == FFIRPC_string || type == FFIRPC_unknown) ret = 1;

    return ret;
}

void ffiRPC_container_free(struct ffiRPC_container_element* element){
    if(ffiRPC_is_pointer(element->type) && element->type != FFIRPC_string){
        switch(element->type){
            case FFIRPC_struct:
                ffiRPC_struct_free(element->data);
                break;
            default: break; //ffiRPC_unknown should not be freed
        }
    } else free(element->data);
}

void ffiRPC_struct_cleanup(ffiRPC_struct_t ffiRPC_struct){
    if(ffiRPC_struct->anti_double_free->size > 0){
        for(size_t i = 0; i < ffiRPC_struct->anti_double_free->capacity; i++){
            if(ffiRPC_struct->anti_double_free->body[i].key != NULL && ffiRPC_struct->anti_double_free->body[i].key != (char*)0xDEAD && ffiRPC_struct->anti_double_free->body[i].value != NULL){
                size_t refcount = 0; //If it stays zero we remove this entry from anti_double_free
                struct ffiRPC_container_element* GC_copy = ffiRPC_struct->anti_double_free->body[i].value;

                for(size_t j = 0; j < ffiRPC_struct->ht->capacity; j++){
                    if(ffiRPC_struct->ht->body[j].key != (char*)0xDEAD && ffiRPC_struct->ht->body[j].key != NULL && ffiRPC_struct->ht->body[j].value != NULL){
                        struct ffiRPC_container_element* check_element = ffiRPC_struct->ht->body[j].value;
                        if(check_element->data == GC_copy->data) refcount++; //belive me, this is how it should be done
                    }
                }

                if(refcount == 0){
                    ffiRPC_container_free(GC_copy);
                    free(GC_copy);

                    char* key_cpy = ffiRPC_struct->anti_double_free->body[i].key;
                    hashtable_remove(ffiRPC_struct->anti_double_free,key_cpy);
                    free(key_cpy);
                }
            }
        }
    }
    ffiRPC_struct->run_GC = 0; //we cleaned up, no need to run it now
}

void ffiRPC_struct_free(ffiRPC_struct_t ffiRPC_struct){
    if(ffiRPC_struct == NULL) return;

    for(size_t i = 0; i < ffiRPC_struct->ht->capacity; i++){
        if(ffiRPC_struct->ht->body[i].key != (char*)0xDEAD && ffiRPC_struct->ht->body[i].key != NULL && ffiRPC_struct->ht->body[i].value != NULL){
            ffiRPC_struct_remove(ffiRPC_struct,ffiRPC_struct->ht->body[i].key);
         }
    }
    ffiRPC_struct_cleanup(ffiRPC_struct); //need to be sure we removed all elements
    hashtable_destroy(ffiRPC_struct->anti_double_free);
    hashtable_destroy(ffiRPC_struct->ht);
    free(ffiRPC_struct);
}

int ffiRPC_struct_unlink(ffiRPC_struct_t ffiRPC_struct, char* key){
    if(ffiRPC_struct == NULL || key == NULL) return 1;

    struct ffiRPC_container_element* element = NULL;
    if((element = hashtable_get(ffiRPC_struct->ht,key)) != NULL){
        if(ffiRPC_is_pointer(element->type)){
            char* free_key = ffiRPC_struct->ht->body[hashtable_find_slot(ffiRPC_struct->ht,key)].key;
            hashtable_remove(ffiRPC_struct->ht,key);

            free(element); //We should free container BUT NOT internals!
            free(free_key); //since key is strdup() ed we should free it
            ffiRPC_struct->size--;
            return 0;
        }
    }
    return 1;
}

int ffiRPC_struct_remove(ffiRPC_struct_t ffiRPC_struct, char* key){
    if(ffiRPC_struct == NULL || key == NULL) return 1;

    struct ffiRPC_container_element* element = NULL;
    if((element = hashtable_get(ffiRPC_struct->ht,key)) != NULL){
        char* free_key = ffiRPC_struct->ht->body[hashtable_find_slot(ffiRPC_struct->ht,key)].key;
        if(element->data != ffiRPC_struct){ //WHY DID YOU DONE THIS?!?!
            hashtable_remove(ffiRPC_struct->ht,key);

            if(ffiRPC_is_pointer(element->type) && element->type != FFIRPC_string){ //ignore strings because they are unique (since strdup'd)
                free(element);
                free(free_key); //since key is strdup() ed we should free it
                ffiRPC_struct->run_GC = 1; //run GC on next ffiRPC_struct_set
            } else {ffiRPC_container_free(element); free(element); free(free_key);}
            ffiRPC_struct->size--;
            return 0;
        }
    }
    return 1;
}

struct ffiRPC_serialise_element{
    char* buf;
    size_t buflen;
    enum ffiRPC_types type;

    char* key;
};
struct ffiRPC_struct_duplicate_info{
    enum ffiRPC_types type;
    char* original_name;
    struct ffiRPC_container_element* original; //may not be original, but it was found first at least

    size_t duplicates_len;
    char** duplicates;
};

struct ffiRPC_struct_duplicate_info* ffiRPC_struct_found_duplicates(ffiRPC_struct_t ffiRPC_struct, size_t* len_output){
    ffiRPC_struct_cleanup(ffiRPC_struct);
    *len_output = ffiRPC_struct->anti_double_free->size;
    if(*len_output > 0){
        int DI = 0;
        struct ffiRPC_struct_duplicate_info* duplicate_info = calloc(*len_output,sizeof(*duplicate_info)); assert(duplicate_info);
        for(size_t i = 0; i < ffiRPC_struct->anti_double_free->capacity; i++){
            size_t start_from = 0;
            if(ffiRPC_struct->anti_double_free->body[i].key != NULL && ffiRPC_struct->anti_double_free->body[i].key != (char*)0xDEAD && ffiRPC_struct->anti_double_free->body[i].value != NULL){
                struct ffiRPC_container_element* GC_copy = ffiRPC_struct->anti_double_free->body[i].value;
                duplicate_info[DI].type = GC_copy->type;

                for(size_t j = 0; j < ffiRPC_struct->ht->capacity; j++){
                    if(ffiRPC_struct->ht->body[j].key != (char*)0xDEAD && ffiRPC_struct->ht->body[j].key != NULL && ffiRPC_struct->ht->body[j].value != NULL){
                        struct ffiRPC_container_element* check_element = ffiRPC_struct->ht->body[j].value;
                        if(duplicate_info[DI].original == NULL){
                            if(check_element->data == GC_copy->data){
                                duplicate_info[DI].original = check_element;
                                duplicate_info[DI].original_name = ffiRPC_struct->ht->body[j].key;
                                start_from = j;
                            }
                        } else if(duplicate_info[DI].original != check_element && duplicate_info[DI].original->data == check_element->data) duplicate_info[DI].duplicates_len++;
                    }
                }
                size_t dupsI = 0;
                for(size_t j = start_from; j < ffiRPC_struct->ht->capacity; j++){
                    if(ffiRPC_struct->ht->body[j].key != (char*)0xDEAD && ffiRPC_struct->ht->body[j].key != NULL && ffiRPC_struct->ht->body[j].value != NULL){
                        assert(duplicate_info[DI].original); //Shit cannot happen there!
                        struct ffiRPC_container_element* check_element = ffiRPC_struct->ht->body[j].value;
                        if(duplicate_info[DI].duplicates == NULL){
                            duplicate_info[DI].duplicates = calloc(duplicate_info[DI].duplicates_len,sizeof(*duplicate_info[DI].duplicates));
                            assert(duplicate_info[DI].duplicates);
                        }

                        if(duplicate_info[DI].original != check_element && duplicate_info[DI].original->data == check_element->data){
                            assert(dupsI != duplicate_info[DI].duplicates_len); //No no no mister pointer you will not go to your SIGSEGV you will go in this ebaniy assert blyat
                            duplicate_info[DI].duplicates[dupsI] = ffiRPC_struct->ht->body[j].key;
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

char* ffiRPC_struct_serialise(ffiRPC_struct_t ffiRPC_struct, size_t* buflen_output){
    hashtable* dupless_ht = malloc(sizeof(*dupless_ht)); assert(dupless_ht);

    dupless_ht->size = ffiRPC_struct->ht->size;
    dupless_ht->capacity = ffiRPC_struct->ht->capacity;
    assert(pthread_mutex_init(&dupless_ht->lock,NULL) == 0);
    dupless_ht->body = malloc(dupless_ht->capacity * sizeof(*dupless_ht->body)); assert(dupless_ht);
    memcpy(dupless_ht->body,ffiRPC_struct->ht->body,sizeof(hashtable_entry) * dupless_ht->capacity);

    size_t dups_len = 0; size_t serialise_elements_len = ffiRPC_struct->size; //we should have space for dublicates but they will be serialised in other manner
    struct ffiRPC_struct_duplicate_info* dups = ffiRPC_struct_found_duplicates(ffiRPC_struct,&dups_len);

    for(size_t i = 0; i < dups_len; i++){
        for(size_t j = 0; j < dups[i].duplicates_len; j++){
            hashtable_remove(dupless_ht,dups[i].duplicates[j]);
        }
    }

    size_t PS_index = 0;
    struct ffiRPC_serialise_element* pre_serialise = calloc(serialise_elements_len,sizeof(*pre_serialise)); assert(pre_serialise);
    for(size_t i = 0; i < dupless_ht->capacity; i++){
        if(dupless_ht->body[i].key != (char*)0xDEAD && dupless_ht->body[i].key != NULL && dupless_ht->body[i].value != NULL){
            struct ffiRPC_container_element* element = dupless_ht->body[i].value;
            pre_serialise[PS_index].type = element->type;
            pre_serialise[PS_index].key = dupless_ht->body[i].key;
            if(ffiRPC_is_pointer(element->type)){
                switch(element->type){
                    case FFIRPC_struct:
                        pre_serialise[PS_index].buf = ffiRPC_struct_serialise(element->data,&pre_serialise[PS_index].buflen);
                        break;
                    case FFIRPC_string:
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
            pre_serialise[PS_index].type = FFIRPC_duplicate;
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

        if(ffiRPC_is_pointer(pre_serialise[i].type) && pre_serialise[i].type != FFIRPC_string){
            switch(pre_serialise[i].type){
                case FFIRPC_struct:
                    free(pre_serialise[i].buf);
                    break;
                default: break;
            }
        }
    }
    free(pre_serialise);
    return buf;
}
ffiRPC_struct_t ffiRPC_struct_unserialise(char* buf){
    assert(buf);

    ffiRPC_struct_t new = ffiRPC_struct_create();
    uint64_t u64_parse_len = *(uint64_t*)buf;

    buf += sizeof(uint64_t);
    struct ffiRPC_serialise_element serialise;

    for(uint64_t i = 0; i < u64_parse_len; i++){
        serialise.key = buf; buf += strlen(serialise.key) + 1;
        serialise.type = *buf; buf++;
        serialise.buflen = *(uint64_t*)buf; buf += sizeof(uint64_t);
        serialise.buf = buf;

        buf += serialise.buflen;
        if(serialise.type == FFIRPC_struct){
            ffiRPC_struct_t unserialised = ffiRPC_struct_unserialise(serialise.buf);

            char NOdoublefree[sizeof(void*) * 2];
            sprintf(NOdoublefree,"%p",unserialised);

            assert(hashtable_get(new->anti_double_free,NOdoublefree) == NULL); //asserting if element unique!

            struct ffiRPC_container_element* element = malloc(sizeof(*element)); assert(element);
            struct ffiRPC_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);

            element->data = unserialised;
            element->type = FFIRPC_struct;
            element->length = 0;
            *GC_copy = *element;

            hashtable_set(new->anti_double_free,strdup(NOdoublefree),GC_copy);
            hashtable_set(new->ht,strdup(serialise.key),element);
        } else if(serialise.type != FFIRPC_duplicate){
            struct ffiRPC_container_element* element = malloc(sizeof(*element)); assert(element);

            element->type = serialise.type;

            element->data = malloc(serialise.buflen); assert(serialise.buflen);
            memcpy(element->data,serialise.buf,serialise.buflen);

            element->length = serialise.buflen;

            hashtable_set(new->ht,strdup(serialise.key),element);
        } else if(serialise.type == FFIRPC_duplicate){
            struct ffiRPC_container_element* element = hashtable_get(new->ht,serialise.buf); //using buf because in buf we wrote "key" of original
            assert(element);

            struct ffiRPC_container_element* DUP_element = malloc(sizeof(*DUP_element)); assert(DUP_element);
            *DUP_element = *element;

            hashtable_set(new->ht,strdup(serialise.key),DUP_element);
        }
        new->size++;
    }
    return new;
}


//REMOVE WHEN DONE!
int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();

    uint64_t input = 12345678;
    ffiRPC_struct_t DFC = ffiRPC_struct_create();
    ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
    ffiRPC_struct_set(ffiRPC_struct,"check_string",(char*)"test 1234567890000000000");
    for(int i = 0; i < 5000; i++){
        char K[1000000];
        sprintf(K,"%d",i);
        ffiRPC_struct_set(ffiRPC_struct,K,DFC);
    }
    ffiRPC_struct_t DFC2 = ffiRPC_struct_create();
    for(int i = 0; i < 5000; i++){
        char K[1000000];
        sprintf(K,"TI%d",i);
        ffiRPC_struct_set(ffiRPC_struct,K,DFC2);
    }
    uint64_t output;
    assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
    assert(output == input);
    assert(ffiRPC_struct_unlink(ffiRPC_struct,"check_int") != 0); //checking that it works properly on int!
    assert(ffiRPC_struct_remove(ffiRPC_struct,"check_int") == 0);
    assert(ffiRPC_struct_set(DFC,"1234",(char*)"some data that should be in this very struct!") == 0);
    assert(ffiRPC_struct_set(ffiRPC_struct,"I1234",(char*)"1234") == 0);

    char* str = NULL;
    ffiRPC_struct_get(ffiRPC_struct,"check_string",str);
    ffiRPC_struct_unlink(ffiRPC_struct,"check_string");
    free(str);

    size_t buflen = 0;
    char* buf = ffiRPC_struct_serialise(ffiRPC_struct,&buflen);
    uint64_t print = *(uint64_t*)buf;
    printf("%lu\n",print);
    FILE* wr = fopen("debug_test_output","wra");
    fwrite(buf,buflen,1,wr);
    fclose(wr);

    ffiRPC_struct_t unser = ffiRPC_struct_unserialise(buf);

    ffiRPC_struct_t unser_C1;
    ffiRPC_struct_get(unser,"0",unser_C1);
    ffiRPC_struct_t unser_C2;
    ffiRPC_struct_get(unser,"TI0",unser_C2);

    for(int i = 1; i < 5000; i++){
        char K[1000000];
        ffiRPC_struct_t C;
        sprintf(K,"%d",i);
        ffiRPC_struct_get(unser,K,C);
        assert(C == unser_C1);

        char* S;
        assert(ffiRPC_struct_get(C,"1234",S) == 0);
        assert(strcmp(S,"some data that should be in this very struct!") == 0);
    }
    for(int i = 1; i < 5000; i++){
        char K[1000000];
        ffiRPC_struct_t C;
        sprintf(K,"TI%d",i);
        ffiRPC_struct_get(unser,K,C);
        assert(C == unser_C2);
    }

    free(buf);

    // *(int*)1 = 0;
    ffiRPC_struct_free(ffiRPC_struct);
    ffiRPC_struct_free(unser);

}
//=================
