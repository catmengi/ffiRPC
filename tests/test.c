#include <ffirpc/rpc_struct.h>
#include <ffirpc/rpc_client.h>
#include <ffirpc/rpc_server.h>
#include <ffirpc/rpc_init.h>
#include <ffirpc/rpc_object.h>

#include <assert.h>
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

    assert(rpc_struct_exists(t,"!c") == 0);
    rpc_struct_t check = NULL;
    assert(rpc_struct_get(t, "!c", check) != 0);
    assert(check == NULL);

    rpc_struct_free(t);
}
#ifdef RPC_SERIALISERS
void check_rpc_struct_ids(){
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


    json_t* test = rpc_struct_serialize(ts);
    char* str = json_dumps(test,JSON_INDENT(1));
    puts(str);

    rpc_struct_t new = rpc_struct_deserialize(test);
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

    free(keys);
    rpc_struct_free(new);
    rpc_struct_free(ts);
    free(str);

    json_decref(test);
}
#endif

void check_copy_of(){
    rpc_struct_t o = rpc_struct_create();
    rpc_struct_t c = rpc_struct_copy(o);

    assert(rpc_struct_whose_copy(c) == NULL); //non trackable object

    rpc_struct_free(o);
    rpc_struct_free(c);

    rpc_struct_t t = rpc_struct_create();
    rpc_struct_t to = rpc_struct_create();

    rpc_struct_set(t,"0",to);

    rpc_struct_t tc = rpc_struct_copy(to);
    assert(rpc_struct_whose_copy(tc));

    rpc_struct_free(to); //addition test;
    rpc_struct_free(t);
    rpc_struct_free(tc);
}
#ifdef RPC_SERIALISERS
void szbuf_test(){
    rpc_struct_t s = rpc_struct_create();

    rpc_struct_set(s, "szbuf", rpc_sizedbuf_create("TEST!",sizeof("TEST!")));

    json_t* ser = rpc_struct_serialize(s);
    rpc_struct_free(s);

    puts("==============================================");
    char* str = json_dumps(ser,JSON_INDENT(1));
    puts(str);
    free(str);

    rpc_struct_t unser = rpc_struct_deserialize(ser);

    rpc_sizedbuf_t szbuf = NULL;
    assert(rpc_struct_get(unser,"szbuf",szbuf) == 0);

    size_t u = 0;
    assert(strcmp(rpc_sizedbuf_getbuf(szbuf,&u),"TEST!") == 0);
    rpc_struct_free(unser);

    json_decref(ser);
}
#endif

void basic_free_test(){
    rpc_struct_t cont = rpc_struct_create();
    rpc_struct_t add = rpc_struct_create();

    char k[sizeof(int) + 8];
    for(int i = 0; i < 256; i++){
        sprintf(k,"%d",i);
        rpc_struct_set(cont,k,add);
    }

    assert(rpc_struct_length(cont) == 256);

    rpc_struct_free(add);
    assert(rpc_struct_length(cont) == 0);
    rpc_struct_free(cont);
}

void adv_free_test(){
    rpc_struct_t cont = rpc_struct_create();

    rpc_struct_t add[16] = {NULL};
    int per_add = 256;

    char k[(sizeof(int) * 2) * 2];
    for(int i = 0; i < sizeof(add) / sizeof(*add); i++){
        add[i] = rpc_struct_create();

        for(int j = 0; j < per_add; j++){
            sprintf(k, "%d %d",i,j);
            rpc_struct_set(cont, k, add[i]);
        }
    }

    for(int i = (sizeof(add) / sizeof(*add)) - 1; i >= 0 ; i--){
        rpc_struct_free(add[i]);
        printf("%s :: %zu == %d\n", __PRETTY_FUNCTION__, rpc_struct_length(cont), per_add * i);
        fflush(stdout);
        assert(rpc_struct_length(cont) == per_add * i);

        for(int j = 0; j <per_add; j++){
            sprintf(k, "%d %d",i,j);
            assert(rpc_struct_exists(cont,k) == 0);
        }
    }
    rpc_struct_free(cont);
}
void adv_free_test_fn(){
    rpc_struct_t cont = rpc_struct_create();

    rpc_function_t add[16] = {NULL};
    int per_add = 256;

    char k[(sizeof(int) * 2) * 2];
    for(int i = 0; i < sizeof(add) / sizeof(*add); i++){
        add[i] = rpc_function_create();

        for(int j = 0; j < per_add; j++){
            sprintf(k, "%d %d",i,j);
            rpc_struct_set(cont, k, add[i]);
        }
    }

    for(int i = (sizeof(add) / sizeof(*add)) - 1; i >= 0 ; i--){
        rpc_function_free(add[i]);
        printf("%s :: %zu == %d\n", __PRETTY_FUNCTION__, rpc_struct_length(cont), per_add * i);
        fflush(stdout);
        assert(rpc_struct_length(cont) == per_add * i);

        for(int j = 0; j <per_add; j++){
            sprintf(k, "%d %d",i,j);
            assert(rpc_struct_exists(cont,k) == 0);
        }
    }
    rpc_struct_free(cont);
}
void adv_free_test_sz(){
    rpc_struct_t cont = rpc_struct_create();

    rpc_sizedbuf_t add[16] = {NULL};
    int per_add = 256;

    char k[(sizeof(int) * 2) * 2];
    for(int i = 0; i < sizeof(add) / sizeof(*add); i++){
        add[i] = rpc_sizedbuf_create("123",4);

        for(int j = 0; j < per_add; j++){
            sprintf(k, "%d %d",i,j);
            rpc_struct_set(cont, k, add[i]);
        }
    }

    for(int i = (sizeof(add) / sizeof(*add)) - 1; i >= 0 ; i--){
        rpc_sizedbuf_free(add[i]);
        printf("%s :: %zu == %d\n", __PRETTY_FUNCTION__, rpc_struct_length(cont), per_add * i);
        fflush(stdout);
        assert(rpc_struct_length(cont) == per_add * i);

        for(int j = 0; j <per_add; j++){
            sprintf(k, "%d %d",i,j);
            assert(rpc_struct_exists(cont,k) == 0);
        }
    }
    rpc_struct_free(cont);
}

rpc_struct_t test(rpc_struct_t rpc,rpc_struct_t u){
    assert(rpc == u);
    puts("TEST FUNCTION SUCCESSFULLY INVOKED!");
    rpc_struct_set(rpc, "test_str", (char*)"gjweruiojgikouerwjgioerejgerio");
    return rpc;
}
void obj_init(){
    rpc_struct_t new_cobj = rpc_struct_create();

    rpc_function_t fn = rpc_function_create();
    rpc_function_set_return_type(fn,RPC_struct);
    rpc_function_set_prototype(fn,(enum rpc_types[]){RPC_struct,RPC_struct},2);
    rpc_function_set_fnptr(fn,test);

    rpc_struct_set(new_cobj,"puts",fn);
    rpc_cobject_set("console",new_cobj);

}

int main(){
    rpc_init();

#ifdef RPC_SERIALISERS
     check_rpc_struct_ids();
     szbuf_test();
#endif

     check_rpc_struct_onfree_remove();
     check_copy_of();
     basic_free_test();
     adv_free_test();
     adv_free_test_fn();
     adv_free_test_sz();
     obj_init();

#ifdef RPC_NETWORK
     rpc_server_launch_port(2077);
#endif

     rpc_client_t client = rpc_client_create();

#ifndef RPC_NETWORK
     rpc_client_connect_local(client);
#else
     assert(rpc_client_connect_tcp(client,"localhost:2077") == 0);
#endif

     rpc_struct_t cobj = rpc_client_cobject_get(client,"console");

     rpc_function_t fn = NULL;
     assert(rpc_struct_get(cobj, "puts", fn) == 0);

     rpc_struct_t debug = rpc_struct_create();

     rpc_struct_t refdbg = rpc_struct_create();
     rpc_struct_set(refdbg, "dbg", debug);

     rpc_struct_t check = ((rpc_struct_t (*)(rpc_struct_t, rpc_struct_t))rpc_function_get_fnptr(fn))(debug,debug);

     assert(check == debug);

     assert(rpc_struct_exists(check,"test_str") == 1);
     assert(rpc_struct_exists(debug,"test_str") == 1);

     rpc_struct_free(check);

     rpc_struct_free(cobj);
     rpc_client_free(client);
     rpc_struct_free(refdbg);

#ifdef RPC_NETWORK
     rpc_server_stop_port(2077);
#endif
}
