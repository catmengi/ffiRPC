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
