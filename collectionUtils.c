#include <string.h>
#include <ctype.h>
#include "collectionUtils.h"



const char* strCasestr(const char* str, const char* subStr);


int startWithIgnoreCaseContains(char* target, List* collection){
	if(collection != NULL && target != NULL){
		int i;
		int len = 0;
		for(i = 0; i < collection->len; i++){
			len = strlen(collection->list[i]) - 2;
			if(strncasecmp(collection->list[i]+1, target, len) == 0){
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
			if(len == (int)(strlen(collection->list[i]) - 2)){
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
			if(len == (int)(strlen(collection->list[i]) - 2)){
				if(strncmp(target, collection->list[i]+1, len) == 0){
					return 1;
				}
			}
		}
	}
	return 0;
}

int ignoreCaseContainAll(char* target, ListList* collection_list){
	int i, j;
	int containall = 0;
	if(collection_list && target){
		for(i = 0; i < collection_list->len; i++){
			containall = 1;
			for(j = 0; j < collection_list->list[i].len; j++){
				if(!strcasestr(target, collection_list->list[i].list[j]+1)){
					containall = 0;
					break;		
				}
			}
			if(containall == 1){
				return 1;
			}
		}
		return 0;
	}
	return 0;
}

const char* strCasestr(const char* str, const char* subStr)
{
	int len = strlen(subStr) - 1;
	if(len == 0)
	{
		return NULL;          
	}
	while(*str)
	{
		if(strncasecmp(str, subStr, len) == 0)    
		{
			return str;
		}
		str++;
	}
	return NULL;
}
