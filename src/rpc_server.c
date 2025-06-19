#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/rpc_object.h"
#include "../include/rpc_network.h"
#include "../include/C-Thread-Pool/thpool.h"

#include <sys/socket.h>

#include <unistd.h>
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

    // printf("[%s] ", timestamp_buffer); // Print the timestamp

    va_list args; // Declare a variable argument list
    va_start(args, format); // Initialize the argument list
    // vprintf(format, args); // Print the user's message using vprintf
    va_end(args); // Clean up the argument list
}

//=============================================


//RPC server internal data structures! ========

//=============================================

//RPC server global variables! ================
static rpc_struct_t RS_netports = NULL;
static rpc_struct_t RS_methods = NULL; //server methonds, NOT rpc functions, used to handle client's requests;
static threadpool RS_thpool = NULL;
//=============================================

//RPC server static function prototypes! ======
static void message_receiver(rpc_net_person_t person, void* userdata);
static void persondata_init(rpc_net_person_t person, void* userdata);
static void net_job(void* arg_p);
//=============================================

//RPC server methonds prototypes ==============
static int ping(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
static int call(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply);
//=============================================

static void RS_init_methods(){
    rpc_struct_set(RS_methods, "ping", (void*)ping); //some typeof related bug in macro prevent me from setting it without void* cast
    rpc_struct_set(RS_methods, "call", (void*)call);
}

void rpc_server_init(){
    RS_netports = rpc_struct_create();

    RS_methods = rpc_struct_create();
    RS_init_methods();

    RS_thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
}

int rpc_server_launch_port(short port){
    char acc[sizeof(int) * 4];
    sprintf(acc,"%d",(int)port);

    if(rpc_struct_exist(RS_netports,acc)) return 1; //port already launched!!!

    rpc_net_notifier_callback notify = {
        .notify = message_receiver,
        .persondata_init = persondata_init,
        .userdata = NULL,
    };

    rpc_net_holder_t net_port = rpc_net_holder_create(notify);
    rpc_net_holder_accept_on(net_port,create_tcp_listenfd(port));

    assert(rpc_struct_set(RS_netports,acc,net_port) == 0);
    return 0;
}

int rpc_server_stop_port(short port){
    char acc[sizeof(int) * 4];
    sprintf(acc,"%d",(int)port);

    if(rpc_struct_exist(RS_netports,acc)){
        rpc_net_holder_t net_port = NULL;

        assert(rpc_struct_get(RS_netports, acc, net_port) == 0);
        assert(rpc_struct_remove(RS_netports,acc) == 0);

        rpc_net_holder_free(net_port);

        return 0;
    } else return 1;
}

static void message_receiver(rpc_net_person_t person, void* userdata){ //retrive request from request queue and launch them into threadpool
    size_t jobs = rpc_net_person_request_ammount(person);

    for(size_t i = 0; i < jobs; i++){
        time_logger("%s received %zu jobs! \n", __PRETTY_FUNCTION__, jobs);

        rpc_struct_t job_info = rpc_struct_create(); //i was forced to use it, because rpc_struct_t support deletion of rpc_struct from other rpc_structs on rpc_struct_free which may occur
                                                     //when client disconnect but it's job is still in threadpool queue.
                                                     //But this may still cause issues when client disconnected while thread handle it. TODO: find a better way to fix this, but this should work

        rpc_struct_set(job_info, "fd", rpc_net_person_fd(person));
        assert(rpc_struct_set(job_info, "persondata", rpc_struct_copy(rpc_net_persondata(person))) == 0);
        assert(rpc_struct_set(job_info, "request", rpc_net_person_get_request(person)) == 0);

        thpool_add_work(RS_thpool,net_job,job_info);

        printf("\t");
        time_logger("job %zu was succesfully added!\n",i);
    }
}
static void persondata_init(rpc_net_person_t person, void* userdata){
    rpc_struct_t persondata = rpc_net_persondata(person);

    rpc_struct_set(persondata, "lobjects", rpc_struct_create());
    rpc_struct_set(persondata, "fd", rpc_net_person_fd(person));
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
                time_logger("request handler returned error, disconnecting client\n");
                goto error;
            }
        } else {time_logger("%s request %s doesnt exist!\n",__PRETTY_FUNCTION__, request_name);goto error;}
    } else {time_logger("%s no request in message!\n",__PRETTY_FUNCTION__);goto error;}

    time_logger("request parsed succesfully\n");
    rpc_struct_free(job_info);
    if(rpc_net_send(fd,reply) != 0) {rpc_struct_free(reply); goto error_shut;};
    return;

error:
    rpc_struct_free(job_info);
    time_logger("bad request from client!\n");
    if(rpc_net_send(fd,reply) != 0) rpc_struct_free(reply);
error_shut:
    time_logger("connection shuted down!\n");
    shutdown(fd,SHUT_RDWR);
    close(fd);
    return;
}

//RPC server methods functions ====================================================
static int ping(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    rpc_struct_set(reply, "pong", (char*)"pong");
    return 0;
}

static int call(rpc_struct_t person, rpc_struct_t request, rpc_struct_t reply){
    rpc_struct_t lobjects = NULL;
    rpc_struct_get(person, "lobjects", lobjects);

    char* cobj_name = NULL;
    char* fn_name = NULL;
    rpc_struct_t fn_params = NULL;

    if(rpc_struct_get(request, "object",cobj_name) != 0) return 1;
    if(rpc_struct_get(request, "function",fn_name) != 0) return 1;
    if(rpc_struct_get(request, "params",fn_params) != 0) return 1;

    rpc_object_load_locals(lobjects);
    enum rpc_server_errors err = rpc_cobject_call(rpc_cobject_get(cobj_name), fn_name, fn_params, reply);

    if(err != ERR_RPC_OK){
        char* str_err = NULL;

        switch(err){
            case ERR_RPC_DOESNT_EXIST:
                str_err = "ERR_RPC_DOESNT_EXIST";
                puts(str_err);
                break;
            case ERR_RPC_PROTOTYPE_DIFFERENT:
                str_err = "ERR_RPC_PROTOTYPE_DIFFERENT";
                puts(str_err);
                *(int*)1 = 0;
                break;
            default: break;
        }

        rpc_struct_set(reply, "error", str_err);
        return 1;
    }
    return 0;
}
//=================================================================================

