#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"

#include <assert.h>
#include <unistd.h>
int main(){
    rpc_struct_t rstruct = rpc_struct_create();
    rpc_struct_set(rstruct, "dbg",0.1488);

    double output = 0;
    rpc_struct_get(rstruct,"dbg",output);

    assert(output == 0.1488);

    rpc_struct_remove(rstruct,"dbg");
    rpc_struct_t set = rpc_struct_create();
    rpc_struct_set(rstruct, "dbg_p",set);

    rpc_struct_t out = NULL;
    rpc_struct_get(rstruct,"dbg_p",out);
    assert(out == set);

    rpc_struct_refcount_increment(set,1); //test of refcount cleanup system
    rpc_struct_free(rstruct);
    rpc_struct_refcount_decrement(set,1);

}
