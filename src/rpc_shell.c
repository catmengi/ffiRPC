#include "../include/rpc_shell.h"
#include "../include/rpc_struct.h"

#include <pthread.h>

typedef struct{
    char* help_str;
    char* (*cmd)(char** args, int arg_len);
}*rpc_cmd;

struct rpc_shell{
    rpc_struct_t servs;
    rpc_struct_t cmds;
}rpc_shell;


