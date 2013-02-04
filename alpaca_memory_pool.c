#include <malloc.h>
#include <string.h>

#include "alpaca_memory_pool.h"
#define POOL_SIZE 1024*10

alpaca_memory_pool* alpaca_memory_pool_create(){
	alpaca_memory_pool* pool = malloc(POOL_SIZE);
	if(!pool){
		return NULL;
	}
	memset(pool, 0, POOL_SIZE);
	pool->start = (char*)pool + sizeof(alpaca_memory_pool);
	pool->last = (char*)pool + sizeof(alpaca_memory_pool);
	pool->end = (char*)pool + POOL_SIZE;
	pool->max = POOL_SIZE - sizeof(alpaca_memory_pool);
	return pool;
}

void* alpaca_memory_poll_malloc(alpaca_memory_pool* pool, int size){
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

void alpaca_memory_poll_destroy(alpaca_memory_pool* pool){
	free(pool);
	return;
}
