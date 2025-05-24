/* This file was automatically generated.  Do not edit! */
#pragma once
#include <pthread.h>
#include <stdint.h>


typedef struct hashtable hashtable;
void hashtable_destroy(hashtable *t);
typedef struct hashtable_entry hashtable_entry;
hashtable_entry *hashtable_body_allocate(unsigned int capacity);
hashtable *hashtable_create();
void hashtable_remove(hashtable *t,char *key);
void hashtable_resize(hashtable *t,unsigned int capacity);
void hashtable_set(hashtable *t,char *key,void *value);
void *hashtable_get(hashtable *t,char *key);
unsigned int hashtable_find_slot(hashtable *t,char *key);
unsigned long hashtable_hash(char *str);
char** hashtable_get_keys(hashtable* t);
void** hashtable_get_values(hashtable* t);
struct hashtable {
	unsigned int size;
	unsigned int capacity;
	hashtable_entry* body;

	pthread_mutex_t lock;
};
struct hashtable_entry {
	char* key;
	void* value;
};

uint64_t murmur(uint8_t* inbuf,uint32_t keylen);

#define INTERFACE 0
