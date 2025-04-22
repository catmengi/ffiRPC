#include "ffiRPC_struct.h"

ffiRPC_struct_t ffiRPC_struct_create(void){
    ffiRPC_struct_t ffiRPC_struct = (ffiRPC_struct_t)malloc(sizeof(*ffiRPC_struct));
    assert(ffiRPC_struct);

    ffiRPC_struct->ht = hashtable_create();
    ffiRPC_struct->size = 0;

    return ffiRPC_struct;
}

int ffiRPC_is_pointer(enum ffiRPC_types type){
    int ret = 0;
    if(type == ffiRPC_struct || type == ffiRPC_string || type == ffiRPC_unknown) ret = 1;

    return ret;
}
void ffiRPC_container_free(struct ffiRPC_container_element* element){

}


//REMOVE WHEN DONE!
int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();

    uint64_t input = 12345678;
    ffiRPC_struct_set(ffiRPC_struct,"check_int",input);
    uint64_t output;
    assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
    assert(output == input);
}
//=================
