#include <ngx_core.h>

#include "alpaca_memory_pool.h"
#include "md5.h"

char* getmd5(const char* data){
	int i;
	unsigned char md[16];
	char tmp[3]={'\0'};
	char* buf;
	buf = malloc(MD5_LEN*sizeof(char));
	if(!buf){
		return NULL;
	}
	memset(buf, 0, MD5_LEN*sizeof(char));
	MD5((unsigned char*)data,strlen(data),md);
	//printf("%s\n",md);
	for (i = 0; i < 16; i++){
		sprintf(tmp,"%2.2X",md[i]);
		strcat(buf,tmp);
	}
	return buf;
}

char* getmd5frompool(char* buf, const char* data){
	int i;
	unsigned char md[16];
	char tmp[3]={'\0'};
	MD5((unsigned char*)data,strlen(data),md);
	//printf("%s\n",md);
	for (i = 0; i < 16; i++){
		sprintf(tmp,"%2.2X",md[i]);
		strcat(buf,tmp);
	}
	return buf;
}

char* getmd5fromngxpool(ngx_http_request_t *r, const char* data){
	int i;
	unsigned char md[16];
	char tmp[3]={'\0'};
	char* buf;
	buf = ngx_pcalloc(r->pool, MD5_LEN*sizeof(char));
	if(!buf){
		return NULL;
	}
	memset(buf, 0, MD5_LEN*sizeof(char));
	MD5((unsigned char*)data,strlen(data),md);
	printf("%s\n",md);
	for (i = 0; i < 16; i++){
		sprintf(tmp,"%2.2X",md[i]);
		strcat(buf,tmp);
	}
	return buf;
}
