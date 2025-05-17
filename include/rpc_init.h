#pragma once

#define RPC_INIT

typedef void (*init_handler)(void);

void rpc_init_add(char* service, char** after_serivces, int after_serivces_len, init_handler init); //add service
void rpc_init_service_load(char* service_name); //load one service
void rpc_init_load(); //load all services

void rpc_init(); //init RPC library. SHOULD BE CALLED BEFORE EVERYTHING ELSE!
