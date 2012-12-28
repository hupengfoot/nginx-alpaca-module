
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "switchconfig.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "policyconfig.h"
#include "cJSON.h"
#include "alpacaClient.h"

#define ZOOKEEPERBUFSIZE 1024
static zhandle_t *zh;
static char* zookeeper_key[] = {"/alpaca.policy.denyIPAddress"}; 

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf);

void init(ngx_alpaca_client_loc_conf_t *aclc, ngx_http_request_t *r){
	initConfigWatch(aclc, r);
}


void initConfigWatch(ngx_alpaca_client_loc_conf_t *aclc, ngx_http_request_t *r){
	zh = zookeeper_init((char *)aclc->zookeeper_addr.data, watcher, 10000, 0, 0, 0);
	if(!zh){
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				"zookeeper init fail! the address is \"%V\" ",
				aclc->zookeeper_addr);
		return;
	}
	aclc->zh = zh;
	char *buffer = malloc(ZOOKEEPERBUFSIZE);//if malloc fail return what?
	struct Stat stat;
	int buflen = ZOOKEEPERBUFSIZE;
	int rc;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	int i = 0;
	for(i = 0; i< zookeeper_key_length; i++){
		rc = zoo_get(zh, zookeeper_key[i], 1, buffer, &buflen, &stat);
		if(rc){
			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"get key from zookeeper fail! the zookeeper address is \"%V\" ",
				 aclc->zookeeper_addr);//may be should use ngx_str_t
			//fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
		}
		rc = parsebuf(buffer);
		if(rc){
			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"get key from zookeeper but parse fail! the zookeeper address is \"%V\" ",
				 aclc->zookeeper_addr);//may be should use ngx_str_t
			//	fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
		}
		memset(buffer, 0 ,ZOOKEEPERBUFSIZE);
	}
	free(buffer);
}


int parsebuf(char *buf){
	cJSON *json, *tmp_json;
	json=cJSON_Parse(buf);
	if (!json) {
		return -1;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		char** tmp_denyIPAddress = (char**)malloc(sizeof(char*) * itemsize);
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			tmp_denyIPAddress[i] = cJSON_Print(tmp_json);
		}
		policyconfig.denyIPAddress = tmp_denyIPAddress;
	}
	return 0;

}


void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx) {
	printf("Watch some change!!!!!!!\n");
	struct Stat stat;
	char *buffer = malloc(ZOOKEEPERBUFSIZE);
	int buflen = ZOOKEEPERBUFSIZE;
	int rc;
	int i;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	for(i = 0; i < zookeeper_key_length; i++){
		if(*path == *zookeeper_key[i]){
			rc = zoo_get(zh, zookeeper_key[i], 1, buffer, &buflen, &stat);
			if(rc == 0){
				rc = parsebuf(buffer);
				if(rc){
				/*	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
							"get key \"%V\" from zookeeper but parse fail! ",
							zookeeper_key[i]);//may be should use ngx_str_t
*/
				}
			}else{
			/*	ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
						"get key \"%V\" from zookeeper fail! ",
						zookeeper_key[i]);//may be should use ngx_str_t*/
			}
			break;
		}

	}
	free(buffer);
	/*struct Stat stat;
	  char buffer[512];
	  int buflen= sizeof(buffer);
	  int rc = zoo_get(zh, "/hupeng", 1, buffer, &buflen, &stat);
	  printf("get data is %s\n",&buffer);*/
}

int procrequest(ngx_http_request_t *r){
	return 1;
}



