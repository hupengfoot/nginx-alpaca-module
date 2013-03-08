typedef struct{
	int max;
	char* start;
	char* end;
	char* last;
}alpaca_memory_pool;

alpaca_memory_pool* alpaca_memory_pool_create(int size);
void* alpaca_memory_pool_malloc(alpaca_memory_pool* pool, int size);
void alpaca_memory_pool_destroy(alpaca_memory_pool* pool);
