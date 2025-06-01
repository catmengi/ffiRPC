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

#include "../include/rpc_struct.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define RPC_ENCRYTION_KEY_SIZE 0

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

typedef struct{
    enum rpc_protocol msg_type;
    rpc_struct_t msg;
}rpc_msg_t;

enum rpc_protocol rpc_protocol_str_enum(char* req);
const char* rpc_protocol_enum_str(enum rpc_protocol operand);

rpc_struct_t rpc_msg_recv(int fd,char encrypt_key[RPC_ENCRYTION_KEY_SIZE]);
int rpc_msg_send(int fd, rpc_struct_t rpc_struct, char uncrypt_key[RPC_ENCRYTION_KEY_SIZE]);
