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




#include "rpc_thread_context.h"
#include "rpc_struct.h"

#include <ffi.h>

#define ARRAY_SIZE(x) ({sizeof((x)) / sizeof((x)[0]);})

enum rpc_server_errors{
    ERR_RPC_OK,
    ERR_RPC_PROTOTYPE_DIFFERENT,
    ERR_RPC_VARIADIC_NOT_ALLOWED,
    ERR_RPC_FUNCTION_EXIST,
};

int rpc_server_add_function(char* function_name, void* function_ptr,enum rpc_types return_type, enum rpc_types* prototype, int prototype_len);
void rpc_server_remove_function(char* function_name);
