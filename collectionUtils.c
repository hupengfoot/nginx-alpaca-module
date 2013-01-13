#include <string.h>
#include <ctype.h>
#include "collectionUtils.h"
int startWithIgnoreCaseContains(char* target, List* collection){
	if(collection != NULL && target != NULL){
		int i;
		int len = 0;
		for(i = 0; i < collection->len; i++){
			len = strlen(collection->list[i]);
			if(strncasecmp(collection->list[i], target, len) == 0){
				return 1;
			}
		}
	}
	return 0;	
}

int ignoreCaseContains(char* target, List* collection, int len){
	if(collection != NULL && target != NULL){
		int i;
		for(i = 0; i < collection->len; i++){
			if(len == (int)(strlen(collection->list[i]) - 1)){
				if(strncasecmp(target, collection->list[i]+1, len) == 0){
					return 1;
				}
			}
		}
	}
	return 0;
}

int contains(char* target, List* collection, int len){
	if(collection != NULL && target != NULL){
		int i;
		for(i = 0; i < collection->len; i++){
			if(len == (int)(strlen(collection->list[i] + 1) - 1)){
				if(strncmp(target, collection->list[i]+1, len) == 0){
					return 1;
				}
			}
		}
	}
	return 0;
}
