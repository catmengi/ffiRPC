#include "ffiRPC_struct.h"
#include <assert.h>

#include <stdio.h>

int main(){
    ffiRPC_struct_t ffiRPC_struct = ffiRPC_struct_create();
    char str[32];
    for(size_t i = 0; i < 512; i++){
        sprintf(str,"%zu",i);
        ffiRPC_struct_set(ffiRPC_struct,str,(char*)str);
    }
    ffiRPC_struct_set(ffiRPC_struct,(char*)"check_int",(uint64_t)1234567);
    char* ptr;uint64_t output;
    assert(ffiRPC_struct_get(ffiRPC_struct,"62",ptr) == 0);
    assert(ffiRPC_struct_get(ffiRPC_struct,"check_int",output) == 0);
    puts(ptr); printf("%lu check\n",output);
}
