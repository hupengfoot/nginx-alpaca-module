
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
static char* zookeeper_key[] = {"/alpaca.filter.enable", "/alpaca.policy.denyIPAddress", "/alpaca.filter.pushBlockEvent", "/alpaca.filter.mount", "/alpaca.filter.blockByVid", "/alpaca.policy.acceptIPPrefix", "/alpaca.policy.acceptHttpMethod", "/alpaca.policy.denyUserAgent", "/alpaca.policy.denyUserAgentPrefix", "/alpaca.policy.denyIPAddressPrefix", "/alpaca.policy.denyIPAddressRate", "/alpaca.policy.denyUserAgentContainAnd", "/alpaca.policy.denyVisterIDRate", "/alpaca.policy.denyNoVisitorIdURL", "/alpaca.url.clientStatusUrl", "/alpaca.url.clientEnableUrl", "/alpaca.url.clientDisableUrl", "/alpaca.url.clientValidateCodeUrl", "/alpaca.client.heartbeat.interval", "/alpaca.message.denyrate"}; 

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf, char *key);
void setDefault(char *key);

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
	//struct Stat stat;
	int rc;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	int i = 0;
	for(i = 0; i< zookeeper_key_length; i++){
		char *buffer = malloc(ZOOKEEPERBUFSIZE);//if malloc fail return what?
		int buflen = ZOOKEEPERBUFSIZE;
		rc = zoo_get(zh, zookeeper_key[i], 1, buffer, &buflen, NULL);
		if(rc){
			setDefault(zookeeper_key[i]);
			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			  "get key from zookeeper fail! the zookeeper address is \"%V\" ",
			  aclc->zookeeper_addr);//may be should use ngx_str_t
			//fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
		}else{
			rc = parsebuf(buffer, zookeeper_key[i]);
			if(rc){
				setDefault(zookeeper_key[i]);
				/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				  "get key from zookeeper but parse fail! the zookeeper address is \"%V\" ",
				  aclc->zookeeper_addr);//may be should use ngx_str_t
				//	fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
			}
		}
		free(buffer);
	}
}

void setDefault(char *key){

}

char* getCharPInstance(char* buf){
	int len = strlen(buf);
	char* result = (char*)malloc(len);
	if(result == NULL){
		return NULL;
	}
	memcpy(result, buf, len);
	return result;
}

int* getIntPInstanceDigit(char* buf){
	int num = atoi(buf);
	if(num == 0){
		return NULL;
	}
	int* result = (int*)malloc(sizeof(int));
	if(result == NULL){
		return NULL;
	}
	*result = num;
	return result;
}

int* getIntPInstance(char* buf){
	int *result = (int*)malloc(sizeof(int));
	if(result == NULL){
		return NULL;
	}
	if(strcmp(buf, "true") == 0){
		*result = 1;
	}
	else{
		*result = 0;
	}
	return result;
}

char** getCharPPInstance(char* buf, int *len){
	cJSON *json, *tmp_json;
	json=cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		*len = itemsize;
		int i = 0;
		char** result = (char**)malloc(sizeof(char*) * itemsize);
		if(result == NULL){
			cJSON_Delete(json);	
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				cJSON_Delete(json);	
				return NULL;
			}
			result[i] = cJSON_Print(tmp_json);
		}
		return result;
	}

}
int parsebuf(char *buf, char *key){
	if(strcmp(key, "/alpaca.filter.enable") == 0){
		int *tmp_enable = getIntPInstance(buf);
		if(!tmp_enable){
			return -1;
		}
		else{
			if(switchconfig.enable){
				int *before_enable = switchconfig.enable;
				switchconfig.enable = tmp_enable;
				free(before_enable);
			}else{
				switchconfig.enable = tmp_enable;
			}
			return 0;
		}
	}
	else if(strcmp(key, "/alpaca.policy.denyIPAddress") == 0){
		int len = 0;
		char** tmp_denyIPAddress = getCharPPInstance(buf, &len);
		if(!tmp_denyIPAddress){
			return -1;
		}
		else{	
			if(policyconfig.denyIPAddress){
				char** before_denyIPAddress = policyconfig.denyIPAddress;
				policyconfig.denyIPAddress = tmp_denyIPAddress;
				int i;
				for(i = 0; i < policyconfig.denyIPAddress_len; i++){
					free(before_denyIPAddress[i]);
				}
				policyconfig.denyIPAddress_len = len;
				free(before_denyIPAddress);
			}
			else{
				policyconfig.denyIPAddress = tmp_denyIPAddress;
				policyconfig.denyIPAddress_len = len;
			}
		}
		return 0;
	}
	/*else if(strcmp(key,"/alpaca.filter.pushBlockEvent") == 0){
		if(switchconfig.pushBlockEvent){
			int *tmp_pushBlockEvent = (int *)malloc(sizeof(int));
			if(strcmp(buf, "true")){

			}
		}
	}*/
	return -1;

}


void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx) {
	printf("Watch some change!!!!!!!\n");
	struct Stat stat;
	int rc;
	int i;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	for(i = 0; i < zookeeper_key_length; i++){
		if(strcmp(path,zookeeper_key[i]) == 0){
			char *buffer = malloc(ZOOKEEPERBUFSIZE);
			int buflen = ZOOKEEPERBUFSIZE;
			rc = zoo_get(zh, zookeeper_key[i], 1, buffer, &buflen, &stat);
			if(rc == 0){
				rc = parsebuf(buffer, zookeeper_key[i]);
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
			free(buffer);
			break;
		}

	}
	/*struct Stat stat;
	  char buffer[512];
	  int buflen= sizeof(buffer);
	  int rc = zoo_get(zh, "/hupeng", 1, buffer, &buflen, &stat);
	  printf("get data is %s\n",&buffer);*/
}

int procrequest(ngx_http_request_t *r){
	if(switchconfig.enable){
		if(*switchconfig.enable == 1){
			return 1;
		}else{
			return 0;
		}
	}
	else{
		return 0;
	}
}



