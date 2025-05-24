#include "../include/rpc_init.h"
#include "../include/rpc_struct.h"

#include "../include/rpc_thread_context.h"
#include "../include/rpc_server.h"

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

#define init_log(fmt, ...) printf(fmt, __VA_ARGS__);

void rpc_service_add(rpc_struct_t load,rpc_service_t service){
    if(rpc_struct_exist(load,service.name) == 0){
        rpc_struct_t new = rpc_struct_create();

        assert(rpc_struct_set(new,"init_handler",service.init_handler) == 0);

        if(service.dependecies_len > 0){
            rpc_struct_t dependencies = rpc_struct_create();

            char int_key[sizeof(int) * 4];
            for(int i = 0; i < service.dependecies_len; i++){
                sprintf(int_key,"%d",i);

                rpc_struct_t after_which = NULL;
                assert(rpc_struct_exist(load,service.dependecies[i]));

                rpc_struct_set(dependencies,service.dependecies[i],0);
            }
            assert(rpc_struct_set(new,"dependencies",dependencies) == 0);
        }
        rpc_struct_set(load,service.name,new);
    }
}
void rpc_service_add_packet(rpc_struct_t load, rpc_service_t* services, int services_len){
    for(int i = 0; i < services_len; i++){
        rpc_service_add(load,services[i]);
    }
}

void rpc_service_load(rpc_struct_t load,char* service_name){
    rpc_struct_t current_service = NULL;
    if(rpc_struct_get(load,service_name,current_service) == 0){
        rpc_struct_t dependencies = NULL;
        if(rpc_struct_get(current_service,"dependencies",dependencies) == 0){
            char** keys = rpc_struct_keys(dependencies);
            for(size_t i = 0; i < rpc_struct_length(dependencies); i++){
                rpc_service_load(load,keys[i]);
            }
            free(keys);
        }
        init_handler init = NULL;
        assert(rpc_struct_get(current_service,"init_handler",init) == 0);

        init();

        init_log("%s: Loaded service %s\n",__PRETTY_FUNCTION__,service_name);
        rpc_struct_remove(load,service_name); //removing because we no longer need it!
    }
}

void rpc_service_load_all(rpc_struct_t load){
    char** keys = rpc_struct_keys(load);
    for(size_t i = 0; i < rpc_struct_length(load); i++){
        rpc_service_load(load,keys[i]);
    }
    free(keys);
}

//========== RPC_init ==========

static char* rpc_server_dependecies[] = {"rpc_thread_context"};

static rpc_service_t init_rpc_modules[] = {
    {.name = "rpc_thread_context", .dependecies = NULL, .dependecies_len = 0, .init_handler = rpc_init_thread_context},
    {.name = "rpc_server", .dependecies = rpc_server_dependecies, .dependecies_len = sizeof(rpc_server_dependecies) / sizeof(rpc_server_dependecies[0]), .init_handler = rpc_server_init},
};

void rpc_init(){
    rpc_struct_t load = rpc_struct_create();

    rpc_service_add_packet(load,init_rpc_modules,sizeof(init_rpc_modules) / sizeof(init_rpc_modules[0]));
    rpc_service_load_all(load);

    rpc_struct_free(load);
}

