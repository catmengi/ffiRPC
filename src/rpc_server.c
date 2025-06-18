#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/sc_queue.h"
#include "../include/rpc_network.h"
#include "../include/C-Thread-Pool/thpool.h"

#include <sys/socket.h>

#include <unistd.h>
#include <assert.h>

#define COND_EXEC(cond, true_code,false_code) ({if(cond) {true_code;} else {false_code;}})

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
static rpc_struct_t RS_netports = NULL;
static threadpool RS_thpool = NULL;
//=============================================

//RPC server static function prototypes! ======
static void message_receiver(rpc_net_person_t person, void* userdata);
static void net_job(void* arg_p);
//=============================================

void rpc_server_init(){
    RS_netports = rpc_struct_create();
    RS_thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN));
}

int rpc_server_launch_port(short port){
    char acc[sizeof(int) * 4];
    sprintf(acc,"%d",(int)port);

    if(rpc_struct_exist(RS_netports,acc)) return 1; //port already launched!!!

    rpc_net_notifier_callback notify = {
        .notify = message_receiver,
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

static void net_job(void* arg_p){ //handles network requests! Works from threadpool
    time_logger("%s: launched job\n",__PRETTY_FUNCTION__);
    rpc_struct_free(arg_p);
}


