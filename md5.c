#include <openssl/md5.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#define MD5_LEN 33 

char* getmd5(const char* data){
	int i;
	unsigned char md[16];
	char tmp[3]={'\0'};
	char* buf;
	buf = malloc(MD5_LEN*sizeof(char));
	MD5((unsigned char*)data,strlen(data),md);
	printf("%s\n",md);
	for (i = 0; i < 16; i++){
		sprintf(tmp,"%2.2X",md[i]);
		strcat(buf,tmp);
	}
	return buf;
}
