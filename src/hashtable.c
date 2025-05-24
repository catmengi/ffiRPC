/**
 * Hashtable implementation
 * (c) 2011-2019 @marekweb
 *
 * Changes to make it more suitable for ffiRPC project by @catmengi 2024-2025
 *
 * Uses dynamic addressing with linear probing.
 */

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../include/hashtable.h"

/*
 * Interface section used for `makeheaders`.
 */

#define HASHTABLE_INITIAL_CAPACITY 4

/**
 * Compute the hash value for the given string.
 * Implements the djb k=33 hash function.
 */
unsigned long hashtable_hash(char* str)
{
	unsigned long hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
	return hash;
}

/**
 * Find an available slot for the given key, using linear probing.
 */

int hashtable_strcmp_wrap(char* s1, char* s2){
	if((s1 == NULL || s1 == (void*)0xDEAD) || (s2 == NULL || s2 == (void*)0xDEAD)) return 1;
	return strcmp(s1,s2);
}
unsigned int hashtable_find_slot(hashtable* t, char* key) {
	unsigned int index = hashtable_hash(key) % t->capacity;
	unsigned int start_index = index;
	unsigned int first_tombstone = t->capacity; // invalid

	while (1) {
		// Check if the current slot is empty (NULL)
		if (t->body[index].key == NULL) {
			// If we are returning an empty slot, check for tombstones
			return (first_tombstone != t->capacity) ? first_tombstone : index;
		}

		// Check if the key matches
		if (hashtable_strcmp_wrap(t->body[index].key, key) == 0) {
			return index; // Key found
		}

		// Check for tombstone
		if (t->body[index].key == (void*)0xdead && first_tombstone == t->capacity) {
			first_tombstone = index; // Remember the first tombstone found
		}

		// Move to the next index
		index = (index + 1) % t->capacity;

		// Check if we've looped back to the start
		if (index == start_index) {
			break; // Full cycle
		}
	}

	// If we reach here, it means the key was not found, return the first tombstone or the current index
	return (first_tombstone != t->capacity) ? first_tombstone : index;
}

/**
 * Return the item associated with the given key, or NULL if not found.
 */
void* hashtable_get(hashtable* t, char* key)
{
	pthread_mutex_lock(&t->lock);
	int index = hashtable_find_slot(t, key);
	if (t->body[index].key != NULL && t->body[index].key != (char*)0xDEAD) {
		pthread_mutex_unlock(&t->lock);
		return t->body[index].value;
	} else {
		pthread_mutex_unlock(&t->lock);
		return NULL;
	}
}

/**
 * Assign a value to the given key in the table.
 */
void hashtable_set_NL(hashtable* t, char* key, void* value)
{
	int index = hashtable_find_slot(t, key);
	if (t->body[index].key != NULL && t->body[index].key != (char*)0xDEAD) {
		/* Entry exists; update it. */
		t->body[index].value = value;
	} else {
		t->size++;
		/* Create a new  entry */
		if ((float)t->size / t->capacity > 0.8) {
			/* Resize the hash table */
			hashtable_resize(t, t->capacity * 2);
			index = hashtable_find_slot(t, key);
		}
		t->body[index].key = key;
		t->body[index].value = value;
	}
}
void hashtable_set(hashtable* t, char* key, void* value)
{
	pthread_mutex_lock(&t->lock);
	hashtable_set_NL(t,key,value);
	pthread_mutex_unlock(&t->lock);
}


/**
 * Remove a key from the table
 */
void hashtable_remove(hashtable* t, char* key)
{
	pthread_mutex_lock(&t->lock);
	int index = hashtable_find_slot(t, key);
	if (t->body[index].key != NULL && t->body[index].key != (char*)0xDEAD) {
		t->body[index].key = (char*)0xDEAD;
		t->body[index].value = NULL;
		t->size--;
	}
	pthread_mutex_unlock(&t->lock);
}

/**
 * Create a new, empty hashtable
 */
hashtable* hashtable_create()
{
	hashtable* new_ht = malloc(sizeof(hashtable));
	assert(new_ht);

	new_ht->size = 0;
	new_ht->capacity = HASHTABLE_INITIAL_CAPACITY;
	new_ht->body = hashtable_body_allocate(new_ht->capacity);
	assert(new_ht->body);

	pthread_mutex_init(&new_ht->lock,NULL);
	return new_ht;
}

/**
 * Allocate a new memory block with the given capacity.
 */
hashtable_entry* hashtable_body_allocate(unsigned int capacity)
{
	// calloc fills the allocated memory with zeroes
	return (hashtable_entry*)calloc(capacity, sizeof(hashtable_entry));
}

/**
 * Resize the allocated memory.
 */
void hashtable_resize(hashtable* t, unsigned int capacity)
{
	assert(capacity >= t->size);
	unsigned int old_capacity = t->capacity;
	hashtable_entry* old_body = t->body;
	t->body = hashtable_body_allocate(capacity);
	assert(t->body);
	t->capacity = capacity;

	// Copy all the old values into the newly allocated body
	unsigned int copyed_keys = 0;
	for (unsigned int i = 0; i < old_capacity; i++) {
		if (old_body[i].key != NULL && old_body[i].key != (char*)0xDEAD) {
			hashtable_set_NL(t, old_body[i].key, old_body[i].value);
			copyed_keys++;
		}
	}
	t->size -= copyed_keys;
	free(old_body);
}

/**
 * Destroy the table and deallocate it from memory. This does not deallocate the contained items.
 */
void hashtable_destroy(hashtable* t)
{
	free(t->body);
	free(t);
}

/**
 * Get keys from hashtable. This does not copy the keys
*/
char** hashtable_get_keys(hashtable* t){
	char** keys = malloc(t->size * sizeof(char*)); assert(keys);
	unsigned int k = 0;

	for(unsigned int i = 0; i < t->capacity; i++){
		if(t->body[i].key != NULL && t->body[i].key != (char*)0xDEAD){
			keys[k++] = t->body[i].key;
		}
	}
	return keys;

}

void** hashtable_get_values(hashtable* t){
	void** values = malloc(t->size * sizeof(void*)); assert(values);

	unsigned int k = 0;
	for(unsigned int i = 0; i < t->capacity; i++){
		if(t->body[i].key != NULL && t->body[i].key != (char*)0xDEAD){
			values[k++] = t->body[i].value;
		}
	}
	return values;
}

uint64_t murmur(uint8_t* inbuf,uint32_t keylen){
	uint64_t h = (525201411107845655ull);
	for (uint32_t i =0; i < keylen; i++,inbuf++){
		h ^= *inbuf;
		h *= 0x5bd1e9955bd1e995;
		h ^= h >> 47;
	}
	return h;
}
