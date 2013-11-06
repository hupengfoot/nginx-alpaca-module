
#include <curl/curl.h>
#include <zookeeper/zookeeper.h>

#include "switchconfig.h"
#include "commonconfig.h"
#include "blockrequestqueue.h"
#include "urlencode.h"
#include "md5.h"
#include "alpaca_constant.h"
#include "alpaca_zookeeper.h"
#include "alpaca_log.h"
#include "alpaca_heartbeat.h"

extern zhandle_t *zh;
extern u_char* zookeeper_addr;
char* zookeeper_key_tmp[] = ZOOKEEPERWATCHKEYS;

void sendFirewallHeartbeatRequest();
httpParams_pool* multi_malloc_heartbeatRequest(int paramnum);

void* heartbeatcycle(void *arg){
	ngx_cycle_t* cycle = (ngx_cycle_t*)(arg);
	while(1) {
		if(switchconfig->clientHeartbeatEnable){
			ngx_log_error(NGX_LOG_INFO, cycle->log, 0, "hupeng test hello kitty!!!!");
			sendFirewallHeartbeatRequest();
		}
		if(zoo_state(zh)){
			if(is_unrecoverable(zh) == ZINVALIDSTATE){
				zookeeper_close(zh);
				zh = NULL;
				//alpaca_log_wirte(ALPACA_WARN, "get key from zookeeper fail! reconnected...");
				zh = zookeeper_init((char*)zookeeper_addr, watcher, 10000, 0, 0, 0);
				if(!zh){
				//	alpaca_log_wirte(ALPACA_ERROR, "init zookeeper fail");
				}
				int zookeeper_key_length = sizeof(zookeeper_key_tmp)/sizeof(char*);
				int i = 0;
				for(i = 0; i< zookeeper_key_length; i++){
					char keyname[sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key_tmp[i]) + 1];
					sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key_tmp[i]);
					get_zk_value(keyname, i);
				}
			}
		}
		sleep(commonconfig->clientHeartbeatInterval);
	}
	return NULL;
}

//void startHeartbeatThread(ngx_http_request_t *r){
//	if(!heartbeatthreadstart){
//		heartbeatthreadstart = 1;
//		pthread_t tid;
//		tid = pthread_create(&tid, NULL, heartbeatThread, NULL);
//		if(tid){
//			alpaca_log_wirte(ALPACA_ERROR, "start heart beat thread fail");
//			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
//			  "start heart beat thread, error num is \"%d\" ",
//			  tid);*/
//		}
//	}
//}

void sendFirewallHeartbeatRequest(){
	int paramnum = 4;
	httpParams_pool* p =  multi_malloc_heartbeatRequest(paramnum);
	if(!p){
		return;
	}
	CURL *curl;
	char *reqUrl = alpaca_memory_pool_malloc(p->pool, strlen(commonconfig->serverRoot) + strlen(commonconfig->serverHeartbeatUrl) + 1);
	if(!reqUrl){
		httpParams_pool_free(p);
		return;
	}
	char *out = alpaca_memory_pool_malloc(p->pool, DEFAULT_HEARTBEAT_MAX_LENTH);
	if(!out){
		httpParams_pool_free(p);
		return;
	}
	strcpy(p->httpParams[0].key, "clientIP");
	strcpy(p->httpParams[0].value, local_ip);
	strcpy(p->httpParams[1].key, "version");
	strcpy(p->httpParams[1].value, ALPACA_CLIENT_VERSION);
	strcpy(p->httpParams[2].key, "enable");
	if(switchconfig->enable){
		strcpy(p->httpParams[2].value, "true");
	}
	else{
		strcpy(p->httpParams[2].value, "false");
	}
	strcpy(p->httpParams[3].key, TOKEN_KEY);
	char* urlbuf = alpaca_memory_pool_malloc(p->pool, strlen(commonconfig->serverHeartbeatUrl) + strlen(local_ip) + 2);
	if(!urlbuf){
		httpParams_pool_free(p);
		return;
	}
	strcpy(urlbuf, commonconfig->serverHeartbeatUrl);
	strcat(urlbuf, "|");
	strcat(urlbuf, local_ip);
	getmd5frompool(p->httpParams[3].value, urlbuf);
	strcat(p->httpParams[3].value, "\0");

	if(pairUrlEncode(p->httpParams, out, paramnum) == -1){
		httpParams_pool_free(p);
		return;
	}
	//strcpy(out, "hupeng+++++++++++++++++++++++++");
	strcpy(reqUrl, commonconfig->serverRoot);
	strcat(reqUrl, commonconfig->serverHeartbeatUrl);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, reqUrl);
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, out);
        int err = curl_easy_perform(curl);
	if(err == 0){
		printf("ok");
	}
	curl_easy_cleanup(curl);
	httpParams_pool_free(p);
}

httpParams_pool* multi_malloc_heartbeatRequest(int paramnum){
	alpaca_memory_pool* p = alpaca_memory_pool_create(POOL_SIZE);
	if(!p){
		return NULL;
	}
	Pair* httpParams = alpaca_memory_pool_malloc(p, sizeof(Pair)*paramnum);
	if(!httpParams){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[0].key = alpaca_memory_pool_malloc(p, strlen("clientIP") + 1);
	if(!httpParams[0].key){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[0].value = alpaca_memory_pool_malloc(p, strlen(local_ip) + 1);
	if(!httpParams[0].value){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[1].key = alpaca_memory_pool_malloc(p, strlen("version") + 1);
	if(!httpParams[1].key){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[1].value = alpaca_memory_pool_malloc(p, strlen(ALPACA_CLIENT_VERSION) + 1);
	if(!httpParams[1].value){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[2].key = alpaca_memory_pool_malloc(p, strlen("enable") + 1);
	if(!httpParams[2].key){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[2].value = alpaca_memory_pool_malloc(p, 6);
	if(!httpParams[2].value){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[3].key = alpaca_memory_pool_malloc(p, strlen(TOKEN_KEY) + 1);
	if(!httpParams[3].key){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams[3].value = alpaca_memory_pool_malloc(p, MD5_LEN*sizeof(char));
	if(!httpParams[3].value){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	httpParams_pool* pool = alpaca_memory_pool_malloc(p, sizeof(httpParams_pool));
	if(!pool){
		alpaca_memory_pool_destroy(p);
		return NULL;
	}
	pool->httpParams = httpParams;
	pool->pool = p;
	return pool;
}
