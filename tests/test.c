#include "../include/rpc_struct.h"
#include "../include/rpc_server.h"
#include "../include/rpc_init.h"
#include "../include/ptracker.h"
#include "../include/rpc_protocol.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void check_rpc_struct_onfree_remove(){
    rpc_struct_t t = rpc_struct_create();
    rpc_struct_t c = rpc_struct_create();

    rpc_struct_set(t,"!c",c);

    rpc_struct_free(c);

    assert(rpc_struct_exist(t,"!c") == 0);
    rpc_struct_free(t);
}

void check_rpc_struct_ids(){
    // rpc_struct_t s = rpc_struct_create();
    //
    // char* IDo = rpc_struct_id_get(s);
    //
    // size_t u = 0;
    // char* buf = rpc_struct_serialise(s, &u);
    //
    // rpc_struct_t n = rpc_struct_unserialise(buf); free(buf);
    //
    // assert(strcmp(IDo, rpc_struct_id_get(n)) == 0);
    //
    // puts(rpc_struct_id_get(s));
    // puts(rpc_struct_id_get(n));
    //
    // rpc_struct_free(s);
    // rpc_struct_free(n);
    rpc_struct_t ts = rpc_struct_create();

    rpc_struct_set(ts, "int", (int64_t)1488);
    rpc_struct_set(ts, "float", 11.44);
    int c = 0;
    double f = 0;
    rpc_struct_get(ts,"int",c);
    rpc_struct_get(ts, "float",f);

    rpc_struct_t dup = rpc_struct_create();
    rpc_struct_set(ts, "d1",dup);
    rpc_struct_set(ts, "d2",dup);

    double g = 0;
    rpc_struct_get(ts,"int",g);
    rpc_struct_set(dup, "int", 12345678);
    rpc_struct_set(dup, "float2", 0.13);
    rpc_struct_set(ts, "better-than-cjson", 144.0);
    printf("%d %lf %lf\n",c,f,g);;



    rpc_function_t fn = rpc_function_create();
    rpc_function_set_return_type(fn,RPC_number);
    rpc_function_set_prototype(fn,(enum rpc_types[]){RPC_number, RPC_struct},2);

    rpc_struct_set(ts,"fn",fn);


    json_t* test = rpc_struct_serialise(ts);
    char* str = json_dumps(test,JSON_INDENT(1));
    puts(str);

    rpc_struct_t new = rpc_struct_unserialise(test);
    char** keys = rpc_struct_keys(new);

    for(int i = 0; i <rpc_struct_length(new); i++){
        puts(keys[i]);
    }

    assert(rpc_struct_typeof(new, "d2") == RPC_struct);
    assert(rpc_struct_typeof(new, "fn") == RPC_function);

    typeof(144.0) better_than_cjson = 0;
    assert(rpc_struct_get(new,"better-than-cjson",better_than_cjson) == 0);

    rpc_struct_t d1 = NULL;
    rpc_struct_t d2 = NULL;
    rpc_struct_get(new, "d1",d1);
    rpc_struct_get(new, "d2", d2);
    assert(d1 == d2);


    assert(strcmp(rpc_struct_id_get(ts), rpc_struct_id_get(new)) == 0);

    json_decref(test);
    free(keys);
    rpc_struct_free(new);
    rpc_struct_free(ts);
    free(str);
}

void check_copy_of(){
    rpc_struct_t o = rpc_struct_create();
    rpc_struct_t c = rpc_struct_copy(o);

    assert(rpc_struct_whoose_copy(c) == NULL); //non trackable object

    rpc_struct_free(o);
    rpc_struct_free(c);

    rpc_struct_t t = rpc_struct_create();
    rpc_struct_t to = rpc_struct_create();

    rpc_struct_set(t,"0",to);

    rpc_struct_t tc = rpc_struct_copy(to);
    assert(rpc_struct_whoose_copy(tc));

    rpc_struct_free(to); //addition test;
    rpc_struct_free(t);
    rpc_struct_free(tc);
}

void szbuf_test(){
    rpc_struct_t s = rpc_struct_create();

    rpc_struct_set(s, "szbuf", rpc_sizedbuf_create("TEST!",sizeof("TEST!")));

    json_t* ser = rpc_struct_serialise(s);
    rpc_struct_free(s);

    puts("==============================================");
    char* str = json_dumps(ser,JSON_INDENT(1));
    puts(str);
    free(str);

    rpc_struct_t unser = rpc_struct_unserialise(ser);

    rpc_sizedbuf_t szbuf = NULL;
    assert(rpc_struct_get(unser,"szbuf",szbuf) == 0);

    size_t u = 0;
    assert(strcmp(rpc_sizedbuf_getbuf(szbuf,&u),"TEST!") == 0);
    rpc_struct_free(unser);


    json_decref(ser);
}

int main(){
    rpc_init();

     check_rpc_struct_ids();
     check_rpc_struct_onfree_remove();
     check_copy_of();
     szbuf_test();
}
