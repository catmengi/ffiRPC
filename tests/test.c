#include <rpc_struct.h>
#include <rpc_sizedbuf.h>
#include <hashtable.h>
#include <sc_queue.h>

#include <stdatomic.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

int main(){
    rpc_struct_t rpc_struct = rpc_struct_create();

    uint64_t input = 12345678;
    rpc_struct_t DFC = rpc_struct_create();
    rpc_struct_set(rpc_struct,"check_int",input);
    rpc_struct_set(rpc_struct,"check_string",(char*)"test 1234567890000000000");
    char* K = malloc(10000);
    for(int i = 0; i < 5000; i++){
        sprintf(K,"%d",i);
        rpc_struct_set(rpc_struct,K,DFC);
    }
    rpc_struct_t DFC2 = rpc_struct_create();
    for(int i = 0; i < 5000; i++){
        sprintf(K,"TI%d",i);
        rpc_struct_set(rpc_struct,K,DFC2);
    }
    rpc_struct_set(rpc_struct,"szbuf",rpc_sizedbuf_create("hello!",sizeof("hello!")));


    uint64_t output;
    assert(rpc_struct_get(rpc_struct,"check_int",output) == 0);
    assert(output == input);
    assert(rpc_struct_unlink(rpc_struct,"check_int") != 0); //checking that it works properly on int!
    assert(rpc_struct_remove(rpc_struct,"check_int") == 0);
    assert(rpc_struct_set(DFC,"1234",(char*)"some data that should be in this very struct!") == 0);
    assert(rpc_struct_set(rpc_struct,"I1234",(char*)"1234") == 0);

    char* str = NULL;
    rpc_struct_get(rpc_struct,"check_string",str);
    rpc_struct_unlink(rpc_struct,"check_string");
    free(str);

    size_t buflen = 0;
    char* buf = rpc_struct_serialise(rpc_struct,&buflen);
    uint64_t print = *(uint64_t*)buf;
    printf("%lu\n",print);
    FILE* wr = fopen("debug_test_output","wra");
    fwrite(buf,buflen,1,wr);
    fclose(wr);

    rpc_struct_t unser = rpc_struct_unserialise(buf);
    rpc_struct_t copy = rpc_struct_copy(rpc_struct);

    rpc_struct_t unser_C1;
    rpc_struct_get(unser,"0",unser_C1);
    rpc_struct_t unser_C2;
    rpc_struct_get(unser,"TI0",unser_C2);

    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"%d",i);
        rpc_struct_get(unser,K,C);
        assert(C == unser_C1);

        char* S;
        assert(rpc_struct_get(C,"1234",S) == 0);
        assert(strcmp(S,"some data that should be in this very struct!") == 0);
    }
    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"TI%d",i);
        rpc_struct_get(unser,K,C);
        assert(C == unser_C2);
    }

    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"%d",i);
        rpc_struct_get(copy,K,C);

        char* S;
        assert(rpc_struct_get(C,"1234",S) == 0);
        assert(strcmp(S,"some data that should be in this very struct!") == 0);
        rpc_struct_remove(copy,K);
    }
    for(int i = 1; i < 5000; i++){
        rpc_struct_t C = NULL;
        sprintf(K,"TI%d",i);
        rpc_struct_get(copy,K,C);
        rpc_struct_remove(copy,K);
    }

    free(buf);

    // *(int*)1 = 0;
    size_t UN = 0;
    free(rpc_struct_serialise(copy,&UN));
    rpc_struct_free(rpc_struct_copy(rpc_struct));
    rpc_struct_free(unser);

    free(rpc_struct_serialise(copy,&UN));
    rpc_struct_t CC = rpc_struct_copy(copy);

    rpc_sizedbuf_t szbuf = NULL;
    rpc_struct_get(CC,"szbuf",szbuf);
    printf("szbuf check! %s\n",rpc_sizedbuf_getbuf(szbuf,&UN));

    rpc_struct_free(copy);
    rpc_struct_free(rpc_struct);
    rpc_struct_free(CC);


    free(K);

}
//=================
