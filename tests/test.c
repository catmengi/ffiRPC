#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/rpc_init.h"
#include "../include/ptracker.h"

#include <assert.h>
#include <math.h>
#include <unistd.h>

struct rpc_struct_duplicate_info;
struct rpc_struct_duplicate_info* rpc_struct_found_duplicates(rpc_struct_t rpc_struct, size_t* len_output);
int main(){
    rpc_init();

    rpc_struct_t cont = rpc_struct_create();
    rpc_struct_t ref = rpc_struct_create();

    rpc_struct_set(cont,"check",ref);
    rpc_struct_free(ref);

    assert(rpc_struct_exist(cont,"check") == 0);

    rpc_struct_t ref2 = rpc_struct_create();
    rpc_struct_set(cont,"check2",ref2);
    for(size_t i = 0; i < pow(2,16); i++){
        printf("%zu\n",i);
        char k[64] = {0};
        sprintf(k,"%zu",i);

        rpc_struct_set(cont,k,ref2);
    }

    size_t len;
    char* buf = rpc_struct_serialise(cont,&len);

    rpc_struct_t unser = rpc_struct_unserialise(buf);
    free(buf);

    rpc_struct_t c = NULL;
    for(size_t i = 0; i < pow(2,16); i++){
        printf("%zu\n",i);
        char k[64] = {0};
        sprintf(k,"%zu",i);

        if(c == NULL){
            rpc_struct_get(unser,k,c);
            assert(c != NULL);
        } else {
            rpc_struct_t tmp = NULL;
            rpc_struct_get(unser,k,tmp);
            assert(tmp == c);
        }
    }
    rpc_struct_free(unser);

    rpc_struct_free(ref2);
    assert(rpc_struct_exist(cont,"check2") == 0);

    rpc_struct_free(cont);


}
