
#include "alpaca_memory_pool.h"

#define BLOCKREQUESTQUEUESIZE 2000
#define PUSH_BLOCK_ARGS_NUM 9

typedef struct{
	char* key;
	char* value;
}Pair;

typedef struct{
	Pair* httpParams;
	alpaca_memory_pool* pool;
}httpParams_pool;

typedef struct httpParams_pool_list{
	httpParams_pool* value;
	struct httpParams_pool_list* next;
}httpParams_pool_list;

typedef struct{
	httpParams_pool* CircularQueue[BLOCKREQUESTQUEUESIZE + 1];
	int head;
	int tail;
	int size;
}BlockRequestQueue;

BlockRequestQueue blockRequestQueue;
httpParams_pool_list* freelist; 

int isBlockQueueEmpty();
int isBlockQueueFull();
int blockQueueOffer(httpParams_pool* e);
httpParams_pool* blockQueuePoll();
void httpParams_pool_free(httpParams_pool* p);
//void freePairP(Pair* pair, int len);
