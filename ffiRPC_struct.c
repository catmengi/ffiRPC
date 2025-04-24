#include "ffiRPC_struct.h"

ffiRPC_struct_t ffiRPC_struct_create(void){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->size = 0;

    return ffiRPC_struct;
}

int ffiRPC_is_pointer(enum ffiRPC_types type){ //return 1 if ffiRPC_type is pointer, 0 if not
    int ret = 0;
    if(type == ffiRPC_struct || type == ffiRPC_string || type == ffiRPC_unknown) ret = 1;

    return ret;
}
void ffiRPC_container_free(struct ffiRPC_container_element* element){
    if(ffiRPC_is_pointer(element->type)){
        switch(element->type){
            case ffiRPC_string:
                free(element->data);
                break;
            case ffiRPC_struct:
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
            ffiRPC_container_free(ffiRPC_struct->ht->body[i].value);
            free(ffiRPC_struct->ht->body[i].value); //freeing ffiRPC_container_element itself
            free(ffiRPC_struct->ht->body[i].key); //All keys are strdup() ed
        }
    }
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
            free(free_key); //since key is strdup() ed we should free it

            free(element); //We should free container BUT NOT internals!
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
        hashtable_remove(ffiRPC_struct->ht,key);
        free(free_key); //since key is strdup() ed we should free it

        ffiRPC_container_free(element);
        free(element);
        return 0;
    }
    return 1;
}

//REMOVE WHEN DONE!
int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();

    uint64_t input = 12345678;
    ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
    ffiRPC_struct_set(ffiRPC_struct,"check_string",(char*)"test 1234567890000000000");
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
