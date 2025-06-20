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




#pragma once

#define RPC_INIT
#include "../include/rpc_struct.h"

typedef void (*init_handler)(void);

typedef struct{
    char* name;

    char** dependecies;
    int dependecies_len;

    init_handler init_handler;
}rpc_service_t;

void rpc_service_add(rpc_struct_t load,rpc_service_t service);
void rpc_service_add_packet(rpc_struct_t load, rpc_service_t* services, int services_len);
void rpc_service_load(rpc_struct_t load,char* service_name);
void rpc_service_load_all(rpc_struct_t load);

void rpc_init(); //init RPC library. SHOULD BE CALLED BEFORE EVERYTHING ELSE!
