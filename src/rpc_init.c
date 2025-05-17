#include "../include/rpc_init.h"
#include "../include/rpc_struct.h"
#include "../include/sc_queue.h"

#include "../include/rpc_thread_context.h"
#include "../include/rpc_server.h"

#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

#define init_log(fmt, ...) printf(fmt, __VA_ARGS__);

rpc_struct_t services = NULL;

void rpc_init_init(){ //should be called from __attribute__(constructor) loader
    if(services == NULL){
        rpc_struct_init(); //need to manualy initialise rpc_struct module because it is easier to use it and not custom data structure
        services = rpc_struct_create();
    }
}

void rpc_init_add(char* service, char** after_serivces, int after_serivces_len, init_handler init){
    if(rpc_struct_exist(services,service) == 0){
        rpc_struct_t new = rpc_struct_create();

        assert(rpc_struct_set(new,"init_handler",init) == 0);

        if(after_serivces_len > 0 && after_serivces != NULL){
            rpc_struct_t dependencies = rpc_struct_create();

            char int_key[sizeof(int) * 4];
            for(int i = 0; i < after_serivces_len; i++){
                sprintf(int_key,"%d",i);

                rpc_struct_t after_which = NULL;
                assert(rpc_struct_exist(services,after_serivces[i]));

                rpc_struct_set(dependencies,after_serivces[i],0);
            }
            assert(rpc_struct_set(new,"dependencies",dependencies) == 0);
        }
        rpc_struct_set(services,service,new);
    }
}

void rpc_init_service_load(char* service_name){
    rpc_struct_t load = NULL;
    if(rpc_struct_get(services,service_name,load) == 0){
        if(rpc_struct_exist(load,"dependencies")){
            rpc_struct_t dependencies = NULL;
            rpc_struct_get(load,"dependencies",dependencies);

            char** keys = rpc_struct_keys(dependencies);
            for(size_t i = 0; i < rpc_struct_length(dependencies); i++){
                rpc_init_service_load(keys[i]);
            }
            free(keys);
        }
        init_handler init = NULL;
        assert(rpc_struct_get(load,"init_handler",init) == 0);

        init();

        init_log("%s: Loaded service %s\n",__FILE__,service_name);
        rpc_struct_remove(services,service_name);
    }
}

void rpc_init_load(){
    char** keys = rpc_struct_keys(services);
    for(size_t i = 0; i < rpc_struct_length(services); i++){
        rpc_init_service_load(keys[i]);
    }
    free(keys);
}

//========== RPC_init ==========

static char* rpc_server_dependencies[] = {"rpc_thread_context"};

void rpc_init(){
    rpc_init_init();

    rpc_init_add("rpc_thread_context",NULL,0,rpc_init_thread_context);
    rpc_init_add("rpc_server",rpc_server_dependencies,sizeof(rpc_server_dependencies) / sizeof(rpc_server_dependencies[0]),rpc_server_init);

    rpc_init_load();
}

