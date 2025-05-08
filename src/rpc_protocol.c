// MIT License
//
// Copyright (c) 2025 Catmengi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.



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
