#include "../include/rpc_struct.h"
#include <assert.h>

int main(){
    rpc_struct_t container = rpc_struct_create();
    rpc_struct_t unlink = rpc_struct_create();

    rpc_struct_set(unlink,"check",123);

    assert(rpc_struct_refcount_increment(unlink,1) != 0);

    rpc_struct_set(container,"unlink",unlink);
    assert(rpc_struct_refcount_increment(unlink,1) == 0);
    rpc_struct_free(container);

    int c = 0;
    rpc_struct_get(unlink,"check",c);
    assert(c == 123);

    assert(rpc_struct_refcount_decrement(unlink,1) == 0);

}
