#include "ffiRPC_struct.h"
#include "hashtable.c/hashtable.h"

ffiRPC_struct_t ffiRPC_struct_create(void){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->anti_double_free = hashtable_create();

    ffiRPC_struct->size = 0;

    return ffiRPC_struct;
}

int ffiRPC_is_pointer(enum ffiRPC_types type){ //return 1 if ffiRPC_type is pointer, 0 if not
    int ret = 0;
    if(type == EffiRPC_struct || type == EffiRPC_string || type == EffiRPC_unknown) ret = 1;

    return ret;
}
void ffiRPC_container_free(struct ffiRPC_container_element* element){
    if(ffiRPC_is_pointer(element->type)){
        switch(element->type){
            case EffiRPC_string:
                free(element->data);
                break;
            case EffiRPC_struct:
                ffiRPC_struct_free(element->data);
                break;
            default: break; //ffiRPC_unknown should not be freed
        }
    } else {
        free(element->data);
    }
}
void ffiRPC_struct_free(ffiRPC_struct_t ffiRPC_struct){
    if(ffiRPC_struct == NULL) return;

    for(size_t i = 0; i < ffiRPC_struct->ht->capacity; i++){
        if(ffiRPC_struct->ht->body[i].key != (char*)0xDEAD && ffiRPC_struct->ht->body[i].key != NULL && ffiRPC_struct->ht->body[i].value != NULL){
            if(((struct ffiRPC_container_element*)ffiRPC_struct->ht->body[i].value)->data != ffiRPC_struct){ //YOU SHOULD NEVER DO THIS!
                char NOdoublefree[sizeof(void*) * 2]; //should be enough;
                sprintf(NOdoublefree,"%p",((struct ffiRPC_container_element*)(ffiRPC_struct->ht->body[i].value))->data);

                if(hashtable_get(ffiRPC_struct->anti_double_free,NOdoublefree) == NULL){ //attempt to make idiot proof free system
                    hashtable_set(ffiRPC_struct->anti_double_free,strdup(NOdoublefree),(void*)0xdead);
                    ffiRPC_container_free(ffiRPC_struct->ht->body[i].value);
                }
                free(ffiRPC_struct->ht->body[i].value); //freeing ffiRPC_container_element itself
            }
            free(ffiRPC_struct->ht->body[i].key); //All keys are strdup() ed
        }
    }
    for(size_t i = 0; i <ffiRPC_struct->anti_double_free->capacity; i++){
        if(ffiRPC_struct->anti_double_free->body[i].key != NULL && ffiRPC_struct->anti_double_free->body[i].key != (char*)0xDEAD){
            free(ffiRPC_struct->anti_double_free->body[i].key);
        }
    }
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
        if(element->data != ffiRPC_struct){ //WHY DID YOU DONE THIS?!?!
            char* free_key = ffiRPC_struct->ht->body[hashtable_find_slot(ffiRPC_struct->ht,key)].key;
            hashtable_remove(ffiRPC_struct->ht,key);

            char NOdoublefree[sizeof(void*) * 2]; //should be enough;
            sprintf(NOdoublefree,"%p",element->data);
            if(hashtable_get(ffiRPC_struct->anti_double_free,NOdoublefree) == NULL){ //idiot proof
                hashtable_set(ffiRPC_struct->anti_double_free,strdup(NOdoublefree),(void*)0xdead);
                ffiRPC_container_free(element);
            }
            free(element);
            free(free_key); //since key is strdup() ed we should free it
            return 0;
        }
    }
    return 1;
}

//REMOVE WHEN DONE!
int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();

    uint64_t input = 12345678;
    ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
    ffiRPC_struct_set(ffiRPC_struct,"check_string",(char*)"test 1234567890000000000");
    ffiRPC_struct_set(ffiRPC_struct,"double free check?",ffiRPC_struct);
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
