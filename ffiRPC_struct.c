#include "ffiRPC_struct.h"
#include "hashtable.c/hashtable.h"

ffiRPC_struct_t ffiRPC_struct_create(void){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->anti_double_free = hashtable_create();

    ffiRPC_struct->size = 0;
    ffiRPC_struct->doubles = 0;

    return ffiRPC_struct;
}

int ffiRPC_is_pointer(enum ffiRPC_types type){ //return 1 if ffiRPC_type is pointer, 0 if not
    int ret = 0;
    if(type == EffiRPC_struct || type == EffiRPC_string || type == EffiRPC_unknown) ret = 1;

    return ret;
}

void ffiRPC_container_free(struct ffiRPC_container_element* element){
    if(ffiRPC_is_pointer(element->type) && element->type != EffiRPC_string){
        switch(element->type){
            case EffiRPC_struct:
                ffiRPC_struct_free(element->data);
                break;
            default: break; //ffiRPC_unknown should not be freed
        }
    } else free(element->data);
}

void ffiRPC_struct_remove_GC(ffiRPC_struct_t ffiRPC_struct){
    if(ffiRPC_struct->anti_double_free->size > 0){
        void** remove_originals = malloc(ffiRPC_struct->ht->size * sizeof(void*)); assert(remove_originals);
        for(size_t i = 0; i < ffiRPC_struct->anti_double_free->capacity; i++){
            int F = 0;
            if(ffiRPC_struct->anti_double_free->body[i].key != NULL && ffiRPC_struct->anti_double_free->body[i].key != (char*)0xDEAD && ffiRPC_struct->anti_double_free->body[i].value != NULL){
                size_t refcount = 0; //If it stays zero we remove this entry from anti_double_free
                struct ffiRPC_container_element* GC_copy = ffiRPC_struct->anti_double_free->body[i].value;
                for(size_t j = 0; j < ffiRPC_struct->ht->capacity; j++){
                    if(ffiRPC_struct->ht->body[j].key != (char*)0xDEAD && ffiRPC_struct->ht->body[j].key != NULL && ffiRPC_struct->ht->body[j].value != NULL){
                        struct ffiRPC_container_element* check_element = ffiRPC_struct->ht->body[j].value;
                        if(check_element->data == GC_copy->data){refcount++; remove_originals[F++] = check_element;} //belive me, this is how it should be done
                    }
                }
                if(refcount == 0){
                    ffiRPC_container_free(GC_copy);
                    free(GC_copy);
                    for(int k = 0; k < F; k++) free(remove_originals[k]);

                    char* key_cpy = ffiRPC_struct->anti_double_free->body[i].key;
                    hashtable_remove(ffiRPC_struct->anti_double_free,key_cpy);
                    free(key_cpy);
                }
            }
        }
        free(remove_originals);
    }
}

void ffiRPC_struct_free(ffiRPC_struct_t ffiRPC_struct){
    if(ffiRPC_struct == NULL) return;

    for(size_t i = 0; i < ffiRPC_struct->ht->capacity; i++){
        if(ffiRPC_struct->ht->body[i].key != (char*)0xDEAD && ffiRPC_struct->ht->body[i].key != NULL && ffiRPC_struct->ht->body[i].value != NULL){
            ffiRPC_struct_remove(ffiRPC_struct,ffiRPC_struct->ht->body[i].key);
         }
    }
    ffiRPC_struct_remove_GC(ffiRPC_struct); //need to be sure we removed all elements
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

            if(ffiRPC_is_pointer(element->type)){
                char NOdoublefree[sizeof(void*) * 2]; //should be enough;
                sprintf(NOdoublefree,"%p",element->data);
                if(hashtable_get(ffiRPC_struct->anti_double_free,NOdoublefree) == NULL){ //idiot proof
                    struct ffiRPC_container_element* GC_copy = malloc(sizeof(*GC_copy)); assert(GC_copy);
                    GC_copy->data = element->data;
                    GC_copy->length = element->length;
                    GC_copy->type = element->type;

                    hashtable_set(ffiRPC_struct->anti_double_free,strdup(NOdoublefree),GC_copy);
                }
                free(element);
                free(free_key); //since key is strdup() ed we should free it
                ffiRPC_struct->doubles++;
                if(ffiRPC_struct->doubles >= ffiRPC_struct->ht->size / (ffiRPC_struct->anti_double_free->size > 0 ? ffiRPC_struct->anti_double_free->size : 1)){
                    ffiRPC_struct_remove_GC(ffiRPC_struct);
                    ffiRPC_struct->doubles = 0;
                }
            } else {ffiRPC_container_free(element); free(element); free(free_key);}
            return 0;
        }
    }
    if(ffiRPC_struct->doubles >= ffiRPC_struct->ht->size / (ffiRPC_struct->anti_double_free->size > 0 ? ffiRPC_struct->anti_double_free->size : 1)){
        ffiRPC_struct_remove_GC(ffiRPC_struct);
        ffiRPC_struct->doubles = 0;
    }
    return 1;
}

//REMOVE WHEN DONE!
int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();

    uint64_t input = 12345678;
    ffiRPC_struct_t DFC = ffiRPC_struct_create();
    ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
    ffiRPC_struct_set(ffiRPC_struct,"check_string",(char*)"test 1234567890000000000");
    for(int i = 0; i < 12500; i++){
        char K[1000000];
        sprintf(K,"%d",i);
        ffiRPC_struct_set(ffiRPC_struct,K,DFC);
    }
    uint64_t output;
    assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
    assert(output == input);
    assert(ffiRPC_struct_unlink(ffiRPC_struct,"check_int") != 0); //checking that it works properly on int!
    assert(ffiRPC_struct_remove(ffiRPC_struct,"check_int") == 0);

    char* str;
    ffiRPC_struct_get(ffiRPC_struct,"check_string",str);
    ffiRPC_struct_unlink(ffiRPC_struct,"check_string");
    free(str);

    ffiRPC_struct_free(ffiRPC_struct);

}
//=================
