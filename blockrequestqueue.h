
#define BLOCKREQUESTQUEUESIZE 2000
#define PUSH_BLOCK_ARGS_NUM 9

typedef struct{
	Pair* CircularQueue[BLOCKREQUESTQUEUESIZE + 1];
	int head;
	int tail;
	int size;
}BlockRequestQueue;

BlockRequestQueue blockRequestQueue;

int isBlockQueueEmpty();
int isBlockQueueFull();
int blockQueueOffer(Pair* e);
Pair* blockQueuePoll();
void freePairP(Pair* pair, int len);
