#include <malloc.h>
#include <string.h>

#include "alpaca_memory_pool.h"

alpaca_memory_pool* alpaca_memory_pool_create(int size){
	alpaca_memory_pool* pool = malloc(size);
	if(!pool){
		return NULL;
	}
	memset(pool, 0, size);
	pool->start = (char*)pool + sizeof(alpaca_memory_pool);
	pool->last = (char*)pool + sizeof(alpaca_memory_pool);
	pool->end = (char*)pool + size;
	pool->max = size - sizeof(alpaca_memory_pool);
	return pool;
}

void* alpaca_memory_pool_malloc(alpaca_memory_pool* pool, int size){//TODO rename
	void* result;
	if(size > (pool->end - pool->last)){
		return NULL;
	}
	else{
		result = pool->last;
		pool->last = pool->last + size;
		return result;
	}
}

void alpaca_memory_pool_destroy(alpaca_memory_pool* pool){ //TODO
	free(pool);
	return;
}

