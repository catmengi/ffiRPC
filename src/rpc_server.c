#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/sc_queue.h"
#include "../include/rpc_thread_context.h"
#include "../include/poll_network.h"
#include "../include/C-Thread-Pool/thpool.h"

#include <jansson.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <ffi.h>

#define LISTEN_MAX 256

typedef struct{
    rpc_struct_t lobjects; //client's local objects, accessed by RS_callable_objects's object ID

    struct sc_queue_ptr cobject_stack;

    int fd;
    //space for future items
}*rpc_client_ctx_t;

typedef struct{
    rpc_client_ctx_t ctx;
    rpc_struct_t job;
}rpc_job_t;

static __thread rpc_client_ctx_t RS_current_ctx = NULL; //threadlocal current context
static rpc_struct_t RS_sobjects = NULL; //named objects that can store any kind of data
static rpc_struct_t RS_cobjects = NULL; //this structure will store rpc_struct_t with rpc_struct_t with client_specific objects and rpc_struct_t with callable functions
static rpc_struct_t RS_ctxmap = NULL;
static rpc_struct_t RS_taskhandlers = NULL;
static threadpool RS_thpool = NULL;
static poll_net_t RS_network = NULL;

static void RS_accept(int, void*);
static void RS_read(int, void*);
static void RS_disconnect(int, void*);
static void RS_server_job(void*);

void rpc_server_init(){
    struct poll_net_callbacks cbs = {
        .accept_cb = RS_accept,
        .disconnect_cb = RS_disconnect,
        .read_cb = RS_read,
        .accept_error_cb = NULL,
    };

    RS_thpool = thpool_init((sysconf(_SC_NPROCESSORS_ONLN) > 0) ? sysconf(_SC_NPROCESSORS_ONLN) : 1);
    RS_taskhandlers = rpc_struct_create();
    RS_sobjects = rpc_struct_create();
    RS_cobjects = rpc_struct_create();
    RS_ctxmap = rpc_struct_create();
    RS_network = poll_net_init(cbs,NULL);
}

int rpc_server_add_shared_object(char* name, rpc_struct_t obj){
    return rpc_struct_set(RS_sobjects, name, obj);
}
rpc_struct_t rpc_server_get_shared_object(char* name){
    rpc_struct_t obj = NULL;
    if(name){
        rpc_struct_get(RS_sobjects, name, obj);
    }
    return obj;
}

int rpc_server_create_callable_object(char* name){
    rpc_struct_t new = rpc_struct_create();
    int ret = rpc_struct_set(RS_cobjects, name,new);

    if(ret == 0){
        assert(rpc_struct_set(new, "local_objects", rpc_struct_create()) == 0);
        assert(rpc_struct_set(new, "functions", rpc_struct_create()) == 0);
    } else rpc_struct_free(new);

    return ret;
}

int rpc_server_callable_object_add_function(char* name, rpc_function_t function){
    rpc_struct_t callable_object = NULL;
    volatile int ret = rpc_struct_get(RS_cobjects, name, callable_object); //i dont know if compiler can mess something up there?

    if(ret == 0){
        rpc_struct_t objects_functions = NULL;
        ret = rpc_struct_get(callable_object, "functions", objects_functions);

        if(ret == 0){
            ret = rpc_struct_set(objects_functions, name, function);
        }

    }
    return ret;
}

int rpc_server_launch_port(short port){
    //TODO: ERROR HANDLING!
    int sockfd = socket(AF_INET, SOCK_STREAM,0);
    assert(sockfd);

    int opt = 1;
    assert(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == 0);

    struct sockaddr_in addr = {
        .sin_family  = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };
    assert(bind(sockfd, (struct sockaddr*)&addr,sizeof(addr)) == 0);
    assert(listen(sockfd, LISTEN_MAX) == 0);

    poll_net_start_accept(RS_network,sockfd);

    return 0;
}


static void RS_accept(int fd, void* udata){
    char fd_s[sizeof(fd) * 4];
    sprintf(fd_s, "%d", fd);

    rpc_client_ctx_t ctx = malloc(sizeof(*ctx)); assert(ctx);

    ctx->lobjects = rpc_struct_create();
    sc_queue_init(&ctx->cobject_stack);
    ctx->fd = fd;

    rpc_struct_set(RS_ctxmap, fd_s,ctx);

    printf("DEBUG: ctx %d created!\n", fd);
}
static void RS_disconnect(int fd, void* udata){
    char fd_s[sizeof(fd) * 4];
    sprintf(fd_s, "%d", fd);

    rpc_client_ctx_t ctx = NULL;
    if(rpc_struct_get(RS_ctxmap, fd_s, ctx) == 0){
        rpc_struct_free(ctx->lobjects);
        free(ctx);

        rpc_struct_remove(RS_ctxmap, fd_s);

        printf("DEBUG: ctx %d removed! \n", fd);
    }
}

static void print_ip(int newfd){
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(newfd, (struct sockaddr *)&addr, &addr_size);

    printf("IP of %d is %s\n",newfd, inet_ntoa(addr.sin_addr));
}
static void RS_read(int fd, void* udata){
    json_error_t err;

    print_ip(fd);

    json_t* json = json_loadfd(fd,JSON_DISABLE_EOF_CHECK | JSON_DECODE_ANY,&err);
    if(json == NULL){
        printf("JSON ERROR! %d:%d %s\n",err.line, err.position, err.text);
        shutdown(fd, SHUT_RDWR);
        close(fd);

        puts("");
        return;
    }
    rpc_struct_t recv_job = rpc_struct_unserialise(json);
    if(recv_job == NULL){
        printf("INVALID JSON FORMAT!\n");

        //TODO: make OS agnostic interface
        shutdown(fd, SHUT_RDWR);
        close(fd);
        //================================

        puts("");
        return;
    }
    rpc_client_ctx_t ctx = NULL;

    char fd_s[sizeof(fd) * 4];
    sprintf(fd_s, "%d", fd);
    if(rpc_struct_get(RS_ctxmap, fd_s, ctx) != 0){
        printf("INTERNAL SERVER ERROR! NO CTX\n");

        rpc_struct_free(recv_job);

        //TODO: make OS agnostic interface
        shutdown(fd, SHUT_RDWR);
        close(fd);
        //================================

        puts("");
        return;
    }
    rpc_job_t* job = malloc(sizeof(*job));assert(job);
    job->ctx = ctx;
    job->job = recv_job;

    thpool_add_work(RS_thpool,RS_server_job,job);

    puts("");
    return;
}

static void RS_server_job(void* arg_p){
    printf("job launched!\n");

    rpc_job_t* job = arg_p;
    RS_current_ctx = job->ctx;

    char* task = NULL;
    if(rpc_struct_get(job->job, "task", task) != 0){
        printf("DEBUG: Bad rpc_struct_t received!, no 'task'!\n");

        //TODO: make OS agnostic interface + allow for local serialisation less connection
        shutdown(RS_current_ctx->fd, SHUT_RDWR);
        close(RS_current_ctx->fd);
        //=================================================================================

        goto exit;
    }

    int (*task_handler)(rpc_job_t* job) = NULL;
    if(rpc_struct_get(RS_taskhandlers, task, task_handler) == 0){
        int ret = task_handler(job);
        if(ret != 0){
            printf("DEBUG: task handler (%s) decided to shutdown the connection!\n",task);

            //TODO: make OS agnostic interface + allow for local serialisation less connection
            shutdown(RS_current_ctx->fd, SHUT_RDWR);
            close(RS_current_ctx->fd);
            //=================================================================================

            goto exit;
        }
    } else {
        printf("DEBUG: unknown task %s\n",task);

        //TODO: make OS agnostic interface + allow for local serialisation less connection
        shutdown(RS_current_ctx->fd, SHUT_RDWR);
        close(RS_current_ctx->fd);
        //=================================================================================

        goto exit;
    }
exit:
    RS_current_ctx = NULL;
    rpc_struct_free(job->job);
    free(job);
}
