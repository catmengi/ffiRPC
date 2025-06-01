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
#include <stdint.h>

#define RPC_PROTOCOL_IDENT "ffiRPC NET v0 beta"

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

rpc_struct_t rpc_msg_recv(int fd,char encrypt_key[RPC_ENCRYTION_KEY_SIZE]){
    char* recv_buf = NULL;
    void* ret = NULL;

    char ident[sizeof(RPC_PROTOCOL_IDENT) + sizeof(uint64_t)] = {0};

    if(recv(fd,ident,sizeof(ident),MSG_NOSIGNAL) != sizeof(ident)) goto exit;
    if(strcmp(ident,RPC_PROTOCOL_IDENT) != 0) goto exit;

    uint64_t recv_len = 0;
    memcpy(&recv_len,&ident[sizeof(RPC_PROTOCOL_IDENT)],sizeof(uint64_t));

    recv_buf = malloc(recv_len); if(recv_buf == NULL) goto exit; //dont using assert() here because we dont want to crash because of random network scanner
    if(recv(fd,recv_buf,(size_t)recv_len,MSG_NOSIGNAL) != recv_len) goto exit;

    ret = rpc_struct_unserialise(recv_buf);

exit:
    free(recv_buf);
    return ret;
}

int rpc_msg_send(int fd, rpc_struct_t rpc_struct, char uncrypt_key[RPC_ENCRYTION_KEY_SIZE]){
    int ret = 1;

    size_t send_len = 0;
    char* send_buf = rpc_struct_serialise(rpc_struct,&send_len);

    char ident[sizeof(RPC_PROTOCOL_IDENT) + sizeof(uint64_t)] = {0};
    memcpy(ident,RPC_PROTOCOL_IDENT,sizeof(RPC_PROTOCOL_IDENT));

    uint64_t u64_sndlen = send_len;
    memcpy(&ident[sizeof(RPC_PROTOCOL_IDENT)],&u64_sndlen,sizeof(uint64_t));

    if(send(fd,ident,sizeof(ident),MSG_NOSIGNAL) != sizeof(ident)) goto exit;
    if(send(fd,send_buf,send_len,MSG_NOSIGNAL) != send_len) goto exit;
    ret = 0;

exit:
    free(send_buf);
    assert(ret == 0);
    return ret;
}
