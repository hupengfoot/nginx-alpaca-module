#include <malloc.h>
#include "policyconfig.h"
#include "blockrequestqueue.h"


int isBlockQueueEmpty(){
	return (blockRequestQueue.head == blockRequestQueue.tail);
}

int isBlockQueueFull(){
	return ((blockRequestQueue.tail + 1) % blockRequestQueue.size == blockRequestQueue.head);
}

int blockQueueOffer(Pair* e){
	if(isBlockQueueFull()){
		Pair* buf = blockQueuePoll();
		freePairP(buf, PUSH_BLOCK_ARGS_NUM);
	}
	blockRequestQueue.CircularQueue[blockRequestQueue.tail] = e;
	blockRequestQueue.tail = (blockRequestQueue.tail + 1) % blockRequestQueue.size;
	return 1;
}

Pair* blockQueuePoll(){
	if(isBlockQueueEmpty()){
		return NULL;
	}
	Pair* result = blockRequestQueue.CircularQueue[blockRequestQueue.head];
	//freePairP(*blockRequestQueue.CircularQueue[blockRequestQueue.head], 8);
	blockRequestQueue.CircularQueue[blockRequestQueue.head] = NULL;
	blockRequestQueue.head = (blockRequestQueue.head + 1) % blockRequestQueue.size;
	return result;
}

void freePairP(Pair* pair, int len){
	int i;
	if(!pair){
		return;
	}
	for(i = 0; i < len; i++){
		if(pair[i].key){
			free(pair[i].key);
		}
		if(pair[i].value){
			free(pair[i].value);
		}
	}
	free(pair);
}
