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



#include <ffirpc/rpc_config.h>
#include <ffirpc/rpc_struct.h>
#include <ffirpc/rpc_server.h>
#include <ffirpc/rpc_object.h>
#include <ffirpc/rpc_object_internal.h>
#include <ffirpc/rpc_network.h>
#include <ffirpc/C-Thread-Pool/thpool.h>

#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdarg.h>
#include <assert.h>

// #define COND_EXEC(cond, true_code,false_code) ({if(cond) {true_code;} else {false_code;}})

//DEBUGING CODE ===============================

void time_logger(const char *format, ...) {
    time_t rawtime;
    struct tm *info;
    char timestamp_buffer[80]; // Buffer to store the formatted timestamp

    time(&rawtime); // Get current time
    info = localtime(&rawtime); // Convert to local time structure

    // Format the time into a string (e.g., "YYYY-MM-DD HH:MM:SS")
    strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y-%m-%d %H:%M:%S", info);

    printf("[%s] ", timestamp_buffer); // Print the timestamp

    va_list args; // Declare a variable argument list
    va_start(args, format); // Initialize the argument list
    vprintf(format, args); // Print the user's message using vprintf
    va_end(args); // Clean up the argument list
}

//=============================================


//RPC server internal data structures! ========

//=============================================

//RPC server global variables! ================
#ifdef RPC_NETWORK
static rpc_struct_t RS_netports = NULL;
static rpc_struct_t RS_persondata = NULL;
static threadpool RS_thpool = NULL;
#endif

static rpc_struct_t RS_methods = NULL; //server methonds, NOT rpc functions, used to handle client's requests;
//=============================================

//RPC server static function prototypes! ======
static void message_receiver(rpc_net_person_t person, void* userdata);
static void persondata_init(rpc_net_person_t person, void* userdata);
static void persondata_destroy(rpc_net_person_t person, void* userdata);
static void net_job(void* arg_p);
//=============================================

//RPC server methonds prototypes ==============
static int ping(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
static int disconnect(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
static int get_cobject(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
static int call(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
static int is_cobject(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
//=============================================

static void RS_init_methods(){
    rpc_struct_set(RS_methods, "ping", (void*)ping); //some typeof related bug in macro prevent me from setting it without void* cast
    rpc_struct_set(RS_methods, "disconnect", (void*)disconnect);
    rpc_struct_set(RS_methods, "get_object",(void*)get_cobject);
    rpc_struct_set(RS_methods, "call", (void*)call);
    rpc_struct_set(RS_methods, "is_cobject", (void*)is_cobject);

    time_logger("%s :: registered %zu methods\n",__PRETTY_FUNCTION__,rpc_struct_length(RS_methods));
    time_logger("%s :: ",__PRETTY_FUNCTION__);

    char** methods = rpc_struct_keys(RS_methods);
    for(size_t i = 0; i < rpc_struct_length(RS_methods); i++){
        printf("%s",methods[i]);
        if(i != rpc_struct_length(RS_methods) - 1) printf(", ");
    }
    free(methods);
    printf("\n\n");
}

void rpc_server_init(){
    #ifdef RPC_NETWORK
    RS_netports = rpc_struct_create();
    RS_persondata = rpc_struct_create();
    RS_thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
    #endif

    RS_methods = rpc_struct_create();
    RS_init_methods();

}

#ifdef RPC_NETWORK
int rpc_server_launch_port(short port){
    char acc[sizeof(int) * 4];
    sprintf(acc,"%d",(int)port);

    if(rpc_struct_exists(RS_netports,acc)) return 1; //port already launched!!!

    pthread_mutex_t* sync_mutex = malloc(sizeof(*sync_mutex)); assert(sync_mutex);
    pthread_mutex_init(sync_mutex,NULL);
    rpc_net_notifier_callback notify = {
        .notify = message_receiver,
        .persondata_init = persondata_init,
        .persondata_destroy = persondata_destroy,
        .userdata = sync_mutex,
    };

    rpc_net_holder_t net_port = rpc_net_holder_create(notify);
    rpc_net_holder_accept_on(net_port,create_tcp_listenfd(port));

    assert(rpc_struct_set(RS_netports,acc,net_port) == 0);
    return 0;
}

int rpc_server_stop_port(short port){
    char acc[sizeof(int) * 4];
    sprintf(acc,"%d",(int)port);

    if(rpc_struct_exists(RS_netports,acc)){
        rpc_net_holder_t net_port = NULL;

        assert(rpc_struct_get(RS_netports, acc, net_port) == 0);
        assert(rpc_struct_remove(RS_netports,acc) == 0);

        pthread_mutex_t* sync_mutex = rpc_net_holder_get_notify(net_port).userdata;
        rpc_net_holder_free(net_port);
        pthread_mutex_destroy(sync_mutex);
        free(sync_mutex);

        return 0;
    } else return 1;
}

static void message_receiver(rpc_net_person_t person, void* userdata){ //retrive request from request queue and launch them into threadpool
    size_t jobs = rpc_net_person_request_ammount(person);

    pthread_mutex_lock(userdata);
    for(size_t i = 0; i < jobs; i++){
        time_logger("%s received %zu jobs! \n", __PRETTY_FUNCTION__, jobs);

        rpc_struct_t job_info = rpc_struct_create(); //i was forced to use it, because rpc_struct_t support deletion of rpc_struct from other rpc_structs on rpc_struct_free which may occur
                                                     //when client disconnect but it's job is still in threadpool queue.
                                                     //But this may still cause issues when client disconnected while thread handle it. TODO: find a better way to fix this, but this should work

        rpc_struct_set(job_info, "fd", rpc_net_person_fd(person));

        rpc_struct_t persondata = NULL;
        assert(rpc_struct_get(RS_persondata, rpc_net_person_id(person), persondata) == 0);
        assert(rpc_struct_set(job_info, "persondata",persondata) == 0);

        assert(rpc_struct_set(job_info, "request", rpc_net_person_get_request(person)) == 0);

        thpool_add_work(RS_thpool,net_job,job_info);

        printf("\t");
        time_logger("job %zu was succesfully added!\n",i);
    }
    pthread_mutex_unlock(userdata);
}
static void persondata_init(rpc_net_person_t person, void* userdata){
    time_logger("new client connected! %d\n", rpc_net_person_fd(person));
    rpc_struct_t persondata = rpc_struct_create();

    rpc_struct_set(persondata, "lobjects", rpc_struct_create());
    rpc_struct_set(persondata, "fd", rpc_net_person_fd(person));

    assert(rpc_struct_set(RS_persondata, rpc_net_person_id(person), persondata) == 0);
}
static void persondata_destroy(rpc_net_person_t person, void* userdata){
    time_logger("client disconnected! %d\n", rpc_net_person_fd(person));
    pthread_mutex_lock(userdata);
    thpool_wait(RS_thpool);
    rpc_struct_remove(RS_persondata, rpc_net_person_id(person));
    pthread_mutex_unlock(userdata);
}

static void net_job(void* arg_p){ //handles network requests! Works from threadpool
    rpc_struct_t job_info = arg_p;
    rpc_struct_t reply = rpc_struct_create();

    rpc_struct_t persondata = NULL;
    rpc_struct_t request = NULL;
    int fd = 0;
    rpc_struct_get(job_info, "fd", fd);
    rpc_struct_get(job_info, "persondata", persondata);
    rpc_struct_get(job_info, "request", request);

    char* request_name = NULL;
    rpc_struct_t request_params = NULL;
    if(rpc_struct_get(request, "method",request_name) == 0){
        int (*request_handler)(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply) = NULL;
        if(rpc_struct_get(RS_methods, request_name,request_handler) == 0){
            rpc_struct_get(request, "params", request_params);
            if(request_handler(persondata,request_params,reply) != 0){
                time_logger("request handler willed to end connection, disconnecting client\n");
                goto error;
            }
        } else {time_logger("%s request %s doesnt exist!\n",__PRETTY_FUNCTION__, request_name);goto error;}
    } else {time_logger("%s no request in message!\n",__PRETTY_FUNCTION__);goto error;}

    time_logger("request parsed succesfully\n");
    rpc_struct_free(job_info);
    if(rpc_net_send(fd,reply) != 0) goto error_shut;
    return;

error:
    rpc_struct_free(job_info);
    time_logger("bad request from client!\n");
    rpc_net_send(fd,reply);
error_shut:
    time_logger("connection shuted down!\n");
    shutdown(fd,SHUT_RDWR);
    close(fd);
    return;
}
#endif

//same as net_job but without network support, will handle requests from local rpc client (i.e in same process)
int rpc_server_localnet_job(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    char* request_name = NULL;
    rpc_struct_t request_params = NULL;

    if(rpc_struct_get(request, "method", request_name) == 0){
        int (*request_handler)(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply) = NULL;
        if(rpc_struct_get(RS_methods, request_name, request_handler) == 0){
            rpc_struct_get(request, "params", request_params);
            return request_handler(person, request_params, reply);
        } else return 1;
    } else return 1;
}

//RPC server methods functions ====================================================
static int ping(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    rpc_struct_set(reply, "pong", (char*)"pong");
    return 0;
}
static int disconnect(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    return 1;
}
static int is_cobject(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    char* ID = NULL;
    int is_cobject = 0;
    if(rpc_struct_get(request,"ID",ID) == 0){
        rpc_struct_t cobj = rpc_cobject_get(ID);

        if(cobj) is_cobject = 1;
    }
    assert(rpc_struct_set(reply,"is_cobject",is_cobject) == 0);

    return 0;
}

static int get_cobject(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    char* cobj_name = NULL;
    if(rpc_struct_get(request, "object", cobj_name) != 0){
        return 1;
    }

    rpc_struct_t cobj = rpc_cobject_get(cobj_name);
    if(cobj == NULL){
        rpc_struct_set(reply, "error", (char*)"ERR_RPC_DOESNT_EXIST");
        return 0; //we doesnt really should end connection here!
    }

    rpc_struct_set(reply,"object", cobj);
    return 0;
}
static int call(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    rpc_struct_t lobjects = NULL;
    rpc_struct_get(person, "lobjects", lobjects);

    char* cobj_name = NULL;
    char* fn_name = NULL;
    rpc_struct_t fn_params = NULL;

    if(rpc_struct_get(request, "object",cobj_name) || rpc_struct_get(request, "function",fn_name) || rpc_struct_get(request, "params",fn_params)) return 1;

    rpc_lobjects_load(lobjects);
    enum rpc_server_errors err = rpc_cobject_call(rpc_cobject_get(cobj_name), fn_name, fn_params, reply);

    if(err != ERR_RPC_OK){
        char* str_err = NULL;

        switch(err){
            case ERR_RPC_DOESNT_EXIST:
                str_err = "ERR_RPC_DOESNT_EXIST";
                break;
            case ERR_RPC_PROTOTYPE_DIFFERENT:
                str_err = "ERR_RPC_PROTOTYPE_DIFFERENT";
                break;
            default: break;
        }
        rpc_struct_set(reply, "error", str_err);
        return 0; //error but not fatal!
    }
    return 0;
}
//=================================================================================

