#include "../include/rpc_protocol.h"
#include "../include/rpc_server.h" //for ARRAY_SIZE

#include <assert.h>

static const char* rpc_protocol_str[] = {
    "RPC_DISCONNECT", "RPC_PING",
    "RPC_AUTH", "RPC_CALL",
    "RPC_MAILBOX_SEND",
    "RPC_RETURN","RPC_MAILBOX_RECV",
    "RPC_OK", "RPC_BAD", "RPC_MALFORMED"
};

enum rpc_protocol rpc_protocol_str_enum(char* req){
    assert(req);
    for(int i = 0; i < ARRAY_SIZE(rpc_protocol_str); i++){
        if(strcmp(rpc_protocol_str[i],req) == 0)
            return i;
    }
    return RPC_MALFORMED; //unknow protocol!
}

const char* rpc_protocol_enum_str(enum rpc_protocol operand){
    assert(operand < ARRAY_SIZE(rpc_protocol_str));
    return rpc_protocol_str[operand];
}
