#pragma once

#include <sys/types.h>

void* mpool_alloc(size_t size);
void mpool_free(void* ptr);
char* mpool_strdup(const char* str);
