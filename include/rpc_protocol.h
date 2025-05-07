enum rpc_protocol{
    //operands from client to server
    RPC_DISCONNECT,
    RPC_PING,
    RPC_AUTH,
    RPC_CALL,
    RPC_MAILBOX_SEND,
    //==============================

    //operands from server to client
    RPC_RETURN,
    RPC_MAILBOX_RECV,
    //==============================

    //operand answers
    RPC_OK,
    RPC_BAD,
    RPC_MALFORMED,
};

enum rpc_protocol rpc_protocol_str_enum(char* req);
const char* rpc_protocol_enum_str(enum rpc_protocol operand);
