#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <ffirpc/rpc_config.h>
#include <ffirpc/memory_pool.h>

#ifdef RPC_POOL_ALLOCATOR

#define CHUNK_MEM_SIZE 64
#define REGION_DEFAULT_CHUNK_AMM 2048
#define ALLOCATOR_DEFAULT_REGIONS 4

typedef struct _chunk{
    struct _chunk *next, *prev;

    void* memory;
    bool used;
}*chunk_t;

typedef struct{
    chunk_t chunks;
    chunk_t last_free;

    size_t num_chunks;
    size_t free_chunks;

    void *mem_start, *mem_end;
}region_t;

typedef struct{
    size_t num_regions;
    region_t* regions;

    pthread_mutex_t lock;
}*allocator_t;

static allocator_t g_pool = NULL;

static void region_init(region_t* region, size_t num_chunks){
    assert(region);

    region->num_chunks = num_chunks;
    region->free_chunks = num_chunks;
    region->last_free = NULL;
    region->mem_start = malloc(region->num_chunks * CHUNK_MEM_SIZE); assert(region->mem_start);
    region->mem_end = region->mem_start + (region->num_chunks * CHUNK_MEM_SIZE);

    region->chunks = calloc(region->num_chunks, sizeof(*region->chunks)); assert(region->chunks);
    for(size_t i = 0; i < region->num_chunks; i++){
        region->chunks[i].memory = region->mem_start + (i * CHUNK_MEM_SIZE);
    }
}

static chunk_t find_free_chunks(region_t* region, size_t num_chunks) {
    if (num_chunks > region->num_chunks) {
        return NULL;
    }

    // Начинаем поиск с last_free, если он валиден, иначе с 0
    size_t start = 0;
    if (region->last_free != NULL) {
        ptrdiff_t offset = region->last_free - region->chunks;
        if (offset >= 0 && (size_t)offset < region->num_chunks) {
            start = (size_t)offset;
        }
    }

    size_t consecutive = 0;
    ssize_t first_idx = -1;

    // Обход всего массива циклически
    for (size_t i = 0; i < region->num_chunks; i++) {
        size_t idx = (start + i) % region->num_chunks;
        chunk_t curr = &region->chunks[idx];

        if (!curr->used) {
            if (consecutive == 0) {
                first_idx = idx;
            }
            consecutive++;
            if (consecutive == num_chunks) {
                // Обновляем last_free для оптимизации следующего поиска
                region->last_free = &region->chunks[(first_idx + num_chunks) % region->num_chunks];
                return &region->chunks[first_idx];
            }
        } else {
            consecutive = 0;
            first_idx = -1;
        }
    }

    // Подходящего блока не найдено
    region->last_free = NULL;
    return NULL;
}



static void* region_alloc(region_t* region, size_t num_chunks){
    void* ret = NULL;
    chunk_t chunk = find_free_chunks(region,num_chunks);

    if(chunk){
        for(size_t i = 0; i < num_chunks; i++){
            if(i > 0){
                chunk[i].prev = &chunk[i - 1];
            }
            if(i < (num_chunks - 1)){
                chunk[i].next = &chunk[i + 1];
            }
            chunk[i].used = true;
        }
        region->free_chunks -= num_chunks;
        ret = chunk->memory;
    }

    return ret;
}

enum{
    REGION_FREE_OK,
    REGION_PTR_OUTSIDE,
    REGION_PTR_INVALID,
    REGION_PTR_NON_ALLOC_START
};
static int region_free(region_t* region, void* ptr){
    if(ptr >= region->mem_start && ptr < region->mem_end){ //inside region
        if((ptr - region->mem_start) == 0 || (ptr - region->mem_start) % CHUNK_MEM_SIZE == 0){ //address is valid
            size_t chunk_index = ptr - region->mem_start == 0 ? 0 : (ptr - region->mem_start) / CHUNK_MEM_SIZE;
            assert(chunk_index < region->num_chunks);

            chunk_t chunk = &region->chunks[chunk_index];
            if(chunk->prev == NULL){
                chunk_t tmp = chunk;
                while(tmp){
                    region->free_chunks++;

                    void* next = tmp->next;
                    tmp->next = NULL;
                    tmp->prev = NULL;
                    tmp->used = false;

                    tmp = next;
                }
                region->last_free = chunk;

                return REGION_FREE_OK;
            } else return REGION_PTR_NON_ALLOC_START;
        } else return REGION_PTR_INVALID;
    } else return REGION_PTR_OUTSIDE;
}

static region_t* allocator_add_region(allocator_t allocator, size_t region_num_chunks){
    size_t old_num_regions = allocator->num_regions++;
    assert((allocator->regions = realloc(allocator->regions,allocator->num_regions * sizeof(*allocator->regions))));
    region_init(&allocator->regions[old_num_regions],region_num_chunks);

    return &allocator->regions[old_num_regions];
}

static region_t* find_or_grow_region(allocator_t allocator, size_t num_chunks){
    region_t* rg = NULL;

    for(size_t i = 0; i < allocator->num_regions; i++){
        region_t* find = &allocator->regions[i];

        if(find->free_chunks >= num_chunks) {
            rg = find;
            break;
        }
    }

    if(rg == NULL){
        rg = allocator_add_region(allocator,num_chunks + REGION_DEFAULT_CHUNK_AMM);
    }
    return rg;
}

static void mpool_create(){
    g_pool = malloc(sizeof(*g_pool)); assert(g_pool);

    g_pool->num_regions = ALLOCATOR_DEFAULT_REGIONS;
    g_pool->regions = malloc(sizeof(*g_pool->regions) * g_pool->num_regions); assert(g_pool->regions);

    for(size_t i = 0; i < g_pool->num_regions; i++){
        region_init(&g_pool->regions[i],REGION_DEFAULT_CHUNK_AMM);
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_pool->lock,&attr);
}

#endif

void* mpool_alloc(size_t size){
    #ifdef RPC_POOL_ALLOCATOR
    void* ret = NULL;
    if(size){
        if(g_pool == NULL) mpool_create();

        pthread_mutex_lock(&g_pool->lock);

        size_t num_chunks = size < CHUNK_MEM_SIZE ? 1 : (size % CHUNK_MEM_SIZE == 0 ? size : (((size / CHUNK_MEM_SIZE) + 1) * CHUNK_MEM_SIZE) / CHUNK_MEM_SIZE);

        region_t* rg = find_or_grow_region(g_pool,num_chunks);
        if((ret = region_alloc(rg,num_chunks)) == NULL){
            assert((ret = region_alloc(allocator_add_region(g_pool,num_chunks + REGION_DEFAULT_CHUNK_AMM),num_chunks)));
        }


        pthread_mutex_unlock(&g_pool->lock);

    }
    return ret;
    #else
    return malloc(size);
    #endif
}

void mpool_free(void* ptr){
    #ifdef RPC_POOL_ALLOCATOR
    if(g_pool && ptr){
        pthread_mutex_lock(&g_pool->lock);

        for(size_t i = 0; i < g_pool->num_regions; i++){
            if(region_free(&g_pool->regions[i],ptr) == REGION_PTR_OUTSIDE) continue;
            else break;
        }

        pthread_mutex_unlock(&g_pool->lock);
    }
    #else
    free(ptr);
    #endif
}

char* mpool_strdup(const char* str){
    assert(str);

    size_t len = strlen(str) + 1;
    char* cpy = mpool_alloc(len);
    memcpy(cpy,str,len);
    return cpy;
}
