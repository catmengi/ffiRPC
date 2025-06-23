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



#include <ffirpc/rpc_client.h>
#include <ffirpc/rpc_struct.h>
#include <ffirpc/rpc_server_internal.h>
#include <ffirpc/rpc_object_internal.h>
#include <ffirpc/rpc_network.h>
#include <ffirpc/poll_network.h>

#include <netdb.h>

#include <ffi.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>


struct _rpc_client{
    void* connection_userdata; //TCP socket or persondata for local connection
    rpc_struct_t (*method_request)(rpc_client_t client, rpc_struct_t request);
    void (*userdata_free)(rpc_client_t client);
    void (*error_handler)(rpc_client_t client, int error);
    void (*disconnect)(rpc_client_t client);
    rpc_struct_t loaded_cobjects;
};

typedef struct{
    int fd;
    pthread_t ping;
    pthread_mutex_t lock;
}*tcp_userdata;

rpc_client_t rpc_client_create(){
    rpc_client_t client = calloc(1,sizeof(*client));
    client->loaded_cobjects = rpc_struct_create();
    assert(client);

    return client;
}
void rpc_client_free(rpc_client_t client){
    rpc_struct_free(client->loaded_cobjects);
    if(client->disconnect) client->disconnect(client);

    free(client);
}

static rpc_struct_t tcp_requestor(rpc_client_t client, rpc_struct_t request){
    tcp_userdata tcp = client->connection_userdata;
    rpc_struct_t reply = NULL;
    pthread_mutex_lock(&tcp->lock);

    if(rpc_net_send(tcp->fd, request) == 0){
        reply = rpc_net_recv(tcp->fd);
        if(reply == NULL){
            if(client->error_handler) client->error_handler(client,ERR_RPC_DISCONNECT);
            shutdown(tcp->fd,SHUT_RDWR);
            close(tcp->fd);
        }
    }

    pthread_mutex_unlock(&tcp->lock);
    return reply;
}
static void* tcp_ping(rpc_client_t client){
    tcp_userdata tcp = client->connection_userdata;

    while(tcp->fd != -1){
        pthread_mutex_lock(&tcp->lock);

        rpc_struct_t request = rpc_struct_create();
        assert(rpc_struct_set(request, "method", (char*)"ping") == 0);
        rpc_struct_free(tcp_requestor(client,request));

        pthread_mutex_unlock(&tcp->lock);
        sleep(TIMEOUT - 1);
    }
    pthread_detach(pthread_self());

    return NULL;
}
static void tcp_disconnect(rpc_client_t client){
    tcp_userdata tcp = client->connection_userdata;
    pthread_mutex_lock(&tcp->lock);
    shutdown(tcp->fd,SHUT_RDWR);
    close(tcp->fd);
    tcp->fd = -1;
    pthread_mutex_unlock(&tcp->lock);
}
int rpc_client_connect_tcp(rpc_client_t client, char* host){
    int ret = 1;
    if(host && client){
        char* mod_host = strdup(host);
        char* port = strchr(mod_host, ':');
        if(port){
            *port = '\0';
            port++;

            struct addrinfo hints = {0};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_NUMERICSERV;

            struct addrinfo *info = NULL;
            if(getaddrinfo(mod_host, port,&hints ,&info) == 0){
                struct addrinfo *iter = info;
                while(iter){
                    int sockfd = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);

                    if(sockfd > 0){
                        if(connect(sockfd, iter->ai_addr, iter->ai_addrlen) == 0){
                            tcp_userdata tcp = malloc(sizeof(*tcp)); assert(tcp);
                            tcp->fd = sockfd;
                            assert(pthread_create(&tcp->ping,NULL,(void* (*)(void*))tcp_ping, client) == 0);

                            pthread_mutexattr_t attr;
                            pthread_mutexattr_init(&attr);
                            pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
                            pthread_mutex_init(&tcp->lock,&attr);

                            client->connection_userdata = tcp;
                            client->method_request = tcp_requestor;
                            client->disconnect = tcp_disconnect;

                            ret = 0;
                            break;
                        } else close(sockfd);
                    }
                    iter = iter->ai_next;
                }
                freeaddrinfo(info);
            }
        }
        free(mod_host);
    }
    return ret; //0 - ok, 1 - bad;
}

static rpc_struct_t local_requestor(rpc_client_t client, rpc_struct_t request){
    int ret = 0;
    rpc_struct_t reply = rpc_struct_create();

    if((ret = rpc_server_localnet_job(client->connection_userdata,request,reply)) != 0){
        rpc_struct_free(reply);
        reply = NULL;
    }
    rpc_struct_free(request);
    return reply;
}
static void local_userdata_free(rpc_client_t client){
    rpc_struct_free(client->connection_userdata);
}

void rpc_client_connect_local(rpc_client_t client){
    client->connection_userdata = rpc_struct_create();

    assert(rpc_struct_set(client->connection_userdata, "lobjects", rpc_struct_create()) == 0);
    client->method_request = local_requestor;
    client->userdata_free = local_userdata_free;
}

typedef struct{
    rpc_client_t client;
    char* fn_name;
    rpc_function_t fetched_fn; //just to same some time on rpc_struct_get
    rpc_struct_t cobj; //parent object of fn

    ffi_cif* cif;
    ffi_closure* closure;
    ffi_type** ffi_prototype;

    pthread_mutex_t lock;
}*rpc_closure_udata;

static void cobj_destructor(rpc_struct_t cobj){
    rpc_struct_manual_lock(cobj);
    char** keys = rpc_struct_keys(cobj);
    size_t cobj_length = rpc_struct_length(cobj);
    for(size_t i = 0; i < cobj_length; i++){
        rpc_closure_udata closure_data = NULL;
        assert(rpc_struct_get(cobj, keys[i], closure_data) == 0);

        ffi_closure_free(closure_data->closure);
        free(closure_data->cif);
        free(closure_data->ffi_prototype);
        free(closure_data);
    }
    free(keys);
    rpc_struct_manual_lock(cobj);
}
typedef struct{
    int was_refcounted; //was it reference counted before we set it into arguments
    void* closure_arg;
}*arg_update_info;

typedef struct{
    void (*free_fn)(void*);
    void* free;
}*manual_free_info;

static void closurefy(rpc_client_t client, rpc_struct_t cobj);
static void call_rpc_closure(ffi_cif* cif, void* ret, void* args[], void* udata){
    rpc_closure_udata my_data = udata;
    pthread_mutex_lock(&my_data->lock);
    rpc_struct_manual_lock(my_data->cobj);

    enum rpc_types* prototype = rpc_function_get_prototype(my_data->fetched_fn);
    int prototype_len = rpc_function_get_prototype_len(my_data->fetched_fn);

    rpc_struct_t request = rpc_struct_create();
    rpc_struct_t call_request = rpc_struct_create();
    rpc_struct_t fn_args = rpc_struct_create();

    hashtable* to_update = hashtable_create();
    hashtable* to_update_ptr = hashtable_create();

    for(int i = 0; i < prototype_len; i++){
        assert(prototype[i] != RPC_none); //check your server bro
        char args_acc[sizeof(int) * 4];
        sprintf(args_acc, "%d", i);

        switch(prototype[i]){
            case RPC_number:
                assert(rpc_struct_set(fn_args,args_acc,(*(uint64_t*)args[i])) == 0);
                break;
            case RPC_real:
                assert(rpc_struct_set(fn_args,args_acc,(*(double*)args[i])) == 0);
                break;
            default:{ //some type system fuckery to not write giant switch-case xD
                assert(rpc_is_pointer(prototype[i]));
                int was_refcounted = rpc_struct_is_refcounted(*(void**)args[i]);
                struct rpc_container_element* element = malloc(sizeof(*element)); assert(element);
                element->type = prototype[i];
                element->data = *(void**)args[i];
                element->length = sizeof(void*); //it is pointer, why not to use void* as size?. NOTE: if this is RPC_string, then rpc_struct_set_internal fill the length for us

                assert(rpc_struct_set_internal(fn_args,args_acc,element) == 0);
                if(was_refcounted == 0 && prototype[i] != RPC_string && prototype[i] != RPC_unknown){
                    rpc_struct_prec_ptr_ctx* ctx = prec_context_get(prec_get(element->data)); assert(ctx); //так надо
                    ctx->free = NULL; //disabling reference counted free for this element if it wasnt reference counted before rpc_struct_set_internal
                }
                char ptr_acc[sizeof(void*) * 4];
                sprintf(ptr_acc, "%p", element->data);

                if(hashtable_get(to_update_ptr,ptr_acc) == NULL || hashtable_get(to_update,args_acc) == NULL){
                    arg_update_info arg_info = malloc(sizeof(*arg_info)); assert(arg_info);
                    arg_info->was_refcounted = was_refcounted;
                    arg_info->closure_arg = args[i];


                    if(hashtable_get(to_update,args_acc) == NULL) hashtable_set(to_update, strdup(args_acc), arg_info);
                    if(hashtable_get(to_update_ptr,ptr_acc) == NULL) hashtable_set(to_update_ptr, strdup(ptr_acc), arg_info);
                }
            }
            break;
        }
    }

    assert(rpc_struct_set(call_request, "object", rpc_struct_id_get(my_data->cobj)) == 0);
    assert(rpc_struct_set(call_request, "function", my_data->fn_name) == 0);
    assert(rpc_struct_set(call_request, "params", fn_args) == 0);

    assert(rpc_struct_set(request, "method", (char*)"call") == 0);
    assert(rpc_struct_set(request, "params", call_request) == 0);

    size_t to_update_size = to_update->size;
    char** to_update_keys = hashtable_get_keys(to_update);
    hashtable* updated = hashtable_create();
    rpc_struct_t reply = my_data->client->method_request(my_data->client, request);
    if(reply){
        enum rpc_types return_type = rpc_struct_typeof(reply,"return");
        assert(return_type == rpc_function_get_return_type(my_data->fetched_fn));

        struct rpc_container_element* element = rpc_struct_get_internal(reply,"return"); //i dont want to deal with type system fuckery or rpc_struct_get_unsafe
        void* return_fill_later = NULL;

        if(return_type != RPC_none){
            if(rpc_is_pointer(return_type)){
                if(return_type != RPC_string && return_type != RPC_unknown){
                    return_fill_later = element->data;
                } else {
                    *(ffi_arg*)ret = (ffi_arg)element->data;
                }
            } else {
                memcpy(ret,element->data, element->length > my_data->cif->rtype->size ? my_data->cif->rtype->size : element->length);
            }
        }

        char upd_p_acc[sizeof(void*) * 4];
        for(size_t i = 0; i < to_update_size; i++){
            arg_update_info info = hashtable_get(to_update, to_update_keys[i]);
            struct rpc_container_element* element = rpc_struct_get_internal(reply,to_update_keys[i]);
            if(element){ //we should expect that not all arguments will be sent back, only thoose which hash was has changed, if you changed it and it wasnt replyed, check hash function of your type!
                sprintf(upd_p_acc, "%p", element->data);
                switch(element->type){
                    case RPC_string:
                        memcpy(*(void**)info->closure_arg, element->data, strlen(element->data) > strlen(*(void**)info->closure_arg) ? strlen(*(void**)info->closure_arg) : strlen(element->data));
                        break;
                    case RPC_struct:{
                        if(hashtable_get(updated, upd_p_acc) == NULL){
                            if(*(void**)info->closure_arg != element->data) //this might happen if we running through local client!
                                rpc_struct_free_internals(*(void**)info->closure_arg);

                            memcpy(*(void**)info->closure_arg, element->data, rpc_struct_memsize());

                            if(element->data == return_fill_later){
                                prec_t rfl_prec = prec_get(return_fill_later);
                                if(rfl_prec){ //because of this check we will ensure that this code will be runned ONCE
                                    if(info == NULL || info->was_refcounted == 0){
                                        rpc_struct_prec_ctx_destroy(rfl_prec,NULL);
                                        prec_delete(rfl_prec); //we dont want this ctx anymore, in return_fill_later we will write pointer of this info;
                                    }
                                    return_fill_later = *(void**)info->closure_arg;
                                }
                            }
                            prec_t ctx_del_prec = prec_get(element->data);
                            if(ctx_del_prec && (info == NULL || info->was_refcounted == 0)){
                                rpc_struct_prec_ptr_ctx* ctx = prec_context_get(ctx_del_prec); assert(ctx);
                                ctx->free = NULL; //remove free function because we need rpc_struct's internal **organs** later
                                prec_delete(ctx_del_prec); //also it will remove it from anywhere else.......
                            } else if(info != NULL && info->was_refcounted == 1) {prec_increment(prec_get(element->data),NULL);}
                            hashtable_set(updated, strdup(upd_p_acc), (void*)1);
                        }
                    }
                    break;
                    case RPC_sizedbuf:{
                        if(hashtable_get(updated, upd_p_acc) == NULL){
                            if(*(void**)info->closure_arg != element->data) //this might happen if we running through local client!
                                rpc_sizedbuf_free_internals(*(void**)info->closure_arg);

                            memcpy(*(void**)info->closure_arg, element->data, rpc_sizedbuf_memsize());

                            if(element->data == return_fill_later){
                                prec_t rfl_prec = prec_get(return_fill_later);
                                if(rfl_prec){ //because of this check we will ensure that this code will be runned ONCE
                                    if(info == NULL || info->was_refcounted == 0){
                                        rpc_struct_prec_ctx_destroy(rfl_prec,NULL);
                                        prec_delete(rfl_prec); //we dont want this ctx anymore, in return_fill_later we will write pointer of this info;
                                    }
                                    return_fill_later = *(void**)info->closure_arg;
                                }
                            }
                            prec_t ctx_del_prec = prec_get(element->data);
                            if(ctx_del_prec && (info == NULL || info->was_refcounted == 0)){
                                rpc_struct_prec_ptr_ctx* ctx = prec_context_get(ctx_del_prec); assert(ctx);
                                ctx->free = NULL; //remove free function because we need rpc_struct's internal **organs** later
                                prec_delete(ctx_del_prec); //also it will remove it from anywhere else.......
                            } else if(info != NULL && info->was_refcounted == 1) prec_increment(prec_get(element->data),NULL);
                            hashtable_set(updated, strdup(upd_p_acc), (void*)1);
                        }
                    }
                    break;
                    case RPC_function:{
                        if(hashtable_get(updated, upd_p_acc) == NULL){
                            if(*(void**)info->closure_arg != element->data) //this might happen if we running through local client!
                                rpc_function_free_internals(*(void**)info->closure_arg);

                            memcpy(*(void**)info->closure_arg, element->data, rpc_function_memsize());

                            if(element->data == return_fill_later){
                                prec_t rfl_prec = prec_get(return_fill_later);
                                if(rfl_prec){ //because of this check we will ensure that this code will be runned ONCE
                                    if(info == NULL || info->was_refcounted == 0){
                                        rpc_struct_prec_ctx_destroy(rfl_prec,NULL);
                                        prec_delete(rfl_prec); //we dont want this ctx anymore, in return_fill_later we will write pointer of this info;
                                    }
                                    return_fill_later = *(void**)info->closure_arg;
                                }
                            }
                            prec_t ctx_del_prec = prec_get(element->data);
                            if(ctx_del_prec && (info == NULL || info->was_refcounted == 0)){
                                rpc_struct_prec_ptr_ctx* ctx = prec_context_get(ctx_del_prec); assert(ctx);
                                ctx->free = NULL; //remove free function because we need rpc_struct's internal **organs** later
                                prec_delete(ctx_del_prec); //also it will remove it from anywhere else.......
                            } else if(info != NULL && info->was_refcounted == 1) prec_increment(prec_get(element->data),NULL);
                            hashtable_set(updated, strdup(upd_p_acc), (void*)1);
                        }
                    }
                    break;


                    default: break; //do nothing, and also to shut-up clangd
                }
            }
        }

        if(return_fill_later){
            char rfl_acc[sizeof(void*) * 4];
            sprintf(rfl_acc, "%p", return_fill_later);

            arg_update_info arg_info = hashtable_get(to_update_ptr,rfl_acc);
            if(arg_info == NULL || arg_info->was_refcounted == 0){
                rpc_struct_prec_ptr_ctx* ctx = prec_context_get(prec_get(return_fill_later));
                if(ctx) ctx->free = NULL; //are you sure? Yes i am. But really, we dont want the return value to be controled by someone else.....
            }

            //TODO: support of returning objects from server as function's retval!

            *(ffi_arg*)ret = (ffi_arg)return_fill_later;
        }
    }else{
        if(my_data->client->error_handler){
            puts("terrible call error happended, we dont even know what xD");
            my_data->client->error_handler(my_data->client, ERR_RPC_CALL_FAIL);
        }
    }

    rpc_struct_free(reply);

    char** to_update_ptrkeys = hashtable_get_keys(to_update_ptr);
    size_t to_update_ptrsize = to_update_ptr->size;
    for(size_t i = 0; i  < to_update_ptrsize; i++){
        hashtable_remove(to_update_ptr,to_update_ptrkeys[i]);
        free(to_update_ptrkeys[i]);
    }
    for(size_t i = 0; i < to_update_size; i++){
        free(hashtable_get(to_update,to_update_keys[i]));
        hashtable_remove(to_update,to_update_keys[i]);
        free(to_update_keys[i]);
    }

    char** updated_keys = hashtable_get_keys(updated);
    size_t updated_size = updated->size;
    for(size_t i = 0; i < updated_size;i++){
        hashtable_remove(updated,updated_keys[i]);
        free(updated_keys[i]);
    }

    free(to_update_ptrkeys);
    free(to_update_keys);
    free(updated_keys);
    hashtable_destroy(to_update_ptr);
    hashtable_destroy(to_update);
    hashtable_destroy(updated);

    pthread_mutex_unlock(&my_data->lock);
    rpc_struct_manual_unlock(my_data->cobj);
}

static void closurefy(rpc_client_t client, rpc_struct_t cobj){
    rpc_struct_manual_lock(cobj); //to be sure that no one will touch it from another thread!

    char** cobj_keys = rpc_struct_keys(cobj);
    rpc_struct_t cif_storage = rpc_struct_create();
    char cif_storage_key[sizeof(size_t) * 4];
    for(size_t i = 0; i < rpc_struct_length(cobj); i++){
        if(rpc_struct_typeof(cobj,cobj_keys[i]) == RPC_function){
            rpc_function_t fn = NULL;
            assert(rpc_struct_get(cobj, cobj_keys[i], fn) == 0);

            rpc_closure_udata closure_data = calloc(1,sizeof(*closure_data)); assert(closure_data);

            void* callable_addr = NULL;
            closure_data->client = client;
            closure_data->closure = ffi_closure_alloc(sizeof(*closure_data->closure), &callable_addr); assert(closure_data->closure);
            closure_data->cif = malloc(sizeof(*closure_data->cif)); assert(closure_data->cif);
            closure_data->cobj = cobj;
            closure_data->fetched_fn = fn;

            pthread_mutexattr_t attr = {0};
            pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_init(&closure_data->lock, &attr);

            closure_data->fn_name = cobj_keys[i]; //if you manualy remove something from cobj then you really know that you would not mess up it, or you a DUMB

            ffi_type** ffi_prototype = malloc(sizeof(*ffi_prototype) *rpc_function_get_prototype_len(fn)); assert(ffi_prototype);
            enum rpc_types* rpc_prototype = rpc_function_get_prototype(fn);
            for(int j = 0; j < rpc_function_get_prototype_len(fn); j++){
                ffi_prototype[j] = rpctype_to_libffi[rpc_prototype[j]];
            }
            closure_data->ffi_prototype = ffi_prototype;

            ffi_type* ffi_return = rpctype_to_libffi[rpc_function_get_return_type(fn)];

            assert(ffi_prep_cif(closure_data->cif, FFI_DEFAULT_ABI,rpc_function_get_prototype_len(fn),ffi_return,ffi_prototype) == FFI_OK);
            assert(ffi_prep_closure_loc(closure_data->closure, closure_data->cif,call_rpc_closure,closure_data,callable_addr) == FFI_OK);

            rpc_function_set_fnptr(fn,callable_addr);

            sprintf(cif_storage_key,"%zu",i);

            rpc_struct_set(cif_storage, cif_storage_key, closure_data);
        }
    }
    free(cobj_keys);
    rpc_struct_set(cobj,"cif_storage",cif_storage);
    rpc_struct_add_destructor(cif_storage,cobj_destructor);
    rpc_struct_manual_unlock(cobj);
}
rpc_struct_t rpc_client_cobject_get(rpc_client_t client, char* cobj_name){
    rpc_struct_t cobj = NULL;
    rpc_struct_t request = rpc_struct_create();
    rpc_struct_t reply = NULL;

    assert(rpc_struct_set(request, "method", (char*)"get_object") == 0);

    rpc_struct_t method_params = rpc_struct_create();
    assert(rpc_struct_set(request,"params", method_params) == 0);
    assert(rpc_struct_set(method_params,"object",cobj_name) == 0);

    reply = client->method_request(client, request);
    if(reply){
        if(rpc_struct_exists(reply, "error") == 0){
            rpc_struct_t reply_cobj = NULL;
            assert(rpc_struct_get(reply, "object",reply_cobj) == 0);

            cobj = rpc_struct_deep_copy(reply_cobj); //copying retrieved object because: 1. free on it will delete it on server 2. modifying it will modify it on server! otherwise it is waste of time

            if(rpc_struct_set(client->loaded_cobjects, rpc_struct_id_get(cobj), cobj) != 0){
                rpc_struct_t old = NULL;
                assert(rpc_struct_get(client->loaded_cobjects, rpc_struct_id_get(cobj), old) == 0);
                rpc_struct_free(old);

                assert(rpc_struct_set(client->loaded_cobjects, rpc_struct_id_get(cobj), cobj) == 0);
            }

            closurefy(client, cobj);
        }
    }
exit:
    rpc_struct_free(reply);
    return cobj;
}
