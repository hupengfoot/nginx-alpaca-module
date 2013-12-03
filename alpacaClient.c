
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <curl/curl.h>
#include <time.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <pthread.h>

#include "switchconfig.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "alpacaClient.h"
#include "urlencode.h"
#include "md5.h"
#include "alpaca_log.h"
#include "alpaca_zookeeper.h"
#include "alpaca_constant.h"
#include "alpaca_heartbeat.h"


extern char* visitId;
extern int allow_ua_empty;
static time_t expiretime;
volatile unsigned long push_event_num;
extern lua_State* L;
extern int volatile denyIPAddressRateExpire;
extern int volatile denyIPVidRateExpire;
extern int volatile denyVisterIDRateExpire;
extern int volatile acceptIPPrefixCount;
extern int send_process_listen_port;
extern char* lua_filename;

void procrequest(ngx_http_request_t *r, Context *context);
int handleInternalRequestIfNeeded(ngx_http_request_t *r, Context *context);
int isFirewallRequest(ngx_http_request_t *r);
Context* getRequestContext(ngx_http_request_t *r);
void handleBlockRequestIfNeeded(Context *context, ngx_http_request_t *r);
int compareDate(char* forbidDate);
int responseIfNeeded(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
void responseStatus(ngx_http_request_t *r, ngx_chain_t **out);
void responseDenyMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
void responseDenyRateMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
char* getResponseDenyMessage(ngx_http_request_t *r, Context *context);
char* getResponseDenyRateMessage(ngx_http_request_t *r, Context *context);
char* getNowLogTime(char* result);
void startPushRequestThread();
void startHealthCheckThread();

char* getHttpStatus(alpaca_memory_pool* pool, enum status s){
	char* buf = alpaca_memory_pool_malloc(pool, sizeof(char) * 4);
	if(!buf){
		return NULL;
	}
	int compute = (int)s;
	sprintf(buf, "%d", compute);
	return buf;
}

void initBlockRequestQueue(){
	blockRequestQueue.head = 0;
	blockRequestQueue.tail = 0;
	blockRequestQueue.size = BLOCKREQUESTQUEUESIZE + 1;
}

void sendFirewallHttpRequest(){
	httpParams_pool* httpParams = blockQueuePoll();
	if(!httpParams){
		return;
	}
	CURL *curl;
	char *reqUrl = alpaca_memory_pool_malloc(httpParams->pool, strlen(commonconfig->serverRoot) + strlen(commonconfig->serverBlockEventUrl) + 1);
	if(!reqUrl){
		return;
	}
	char *out = alpaca_memory_pool_malloc(httpParams->pool, DEFAULT_BLOCK_MAX_LENTH);
	if(!out){
		return;
	}
	strcpy(httpParams->httpParams[8].key, TOKEN_KEY);
	char* urlbuf = alpaca_memory_pool_malloc(httpParams->pool, strlen(commonconfig->serverBlockEventUrl) + strlen(local_ip) + 2);
	if(!urlbuf){
		return;
	}
	strcpy(urlbuf, commonconfig->serverBlockEventUrl);
	strcat(urlbuf, "|");
	strcat(urlbuf, local_ip);
	getmd5frompool(httpParams->httpParams[8].value, urlbuf);
	strcat(httpParams->httpParams[8].value, "\0");
	if(pairUrlEncode(httpParams->httpParams, out, PUSH_BLOCK_ARGS_NUM) == -1){
		return;
	}
	strcpy(reqUrl, commonconfig->serverRoot);
	strcat(reqUrl, commonconfig->serverBlockEventUrl);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, reqUrl);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, out);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	push_event_num ++;

	if(time(NULL) > expiretime){
		while(freelist){
			httpParams_pool_list* freelist_head;
			do{
				freelist_head = freelist;
			}while(!__sync_bool_compare_and_swap(&freelist, freelist_head, freelist_head->next));
			alpaca_memory_pool_destroy(freelist_head->value->pool);		
		}
		expiretime = time(NULL) + EXPIRETIME;
		char push_info[1024];
		char num[129];
		memset(push_info, 0, 1024);
		strcpy(push_info, "have pushed ");
		sprintf(num, "%lu", push_event_num);
		strcat(push_info, num);
		strcat(push_info, " event");
		alpaca_log_wirte(ALPACA_INFO, push_info);
	}
}

void* pushRequestThread(void *arg){
	while(1) {
		int needSleep = 1;
		if(isBlockQueueEmpty() == 0){
			needSleep = 0;
			sendFirewallHttpRequest();
			usleep(5000);
		}
		if(needSleep){
			sleep(1);
		}
	}
	return NULL;
}

void* healthCheckThread(void *arg){
	CURL *curl;
	char url[100];

	denyIPAddressRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
	denyIPVidRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
	denyVisterIDRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
	int first_time = 0 ;
	while(1){
		curl = curl_easy_init();
		ngx_memset(url, 0, 100);
		sprintf(url, "%s:%d/", "http://127.0.0.1", send_process_listen_port);	
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
		curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if(denyIPAddressRateExpire < (int)time(NULL)){
			curl = curl_easy_init();
			sprintf(url, "%s:%d/%s", "http://127.0.0.1", send_process_listen_port, "denyipaddressrate");	
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			char buf[100];
			ngx_memset(buf, 0, 100);
			sprintf(buf, "%s%s", "long time no update denyipaddressrate! ", local_ip);
			alpaca_log_wirte(ALPACA_WARN, buf);
			//	curl = curl_easy_init();
			//	memset(url, 0, 100);
			//	sprintf(url, "%s:%d/hupeng?%s%s", "http://127.0.0.1", DEFAULT_ALARM_SERVICE_LISTEN_PORT, "long_time_no_update_denyipaddressrate!", local_ip);
			//	curl_easy_setopt(curl, CURLOPT_URL, url);
			//	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			//	curl_easy_perform(curl);
			//	curl_easy_cleanup(curl);
		}
		if(denyIPVidRateExpire < (int)time(NULL)){
			curl = curl_easy_init();
			sprintf(url, "%s:%d/%s", "http://127.0.0.1", send_process_listen_port, "denyIPVidRate");
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			char buf[100];
			ngx_memset(buf, 0, 100);
			sprintf(buf, "%s%s", "long time no update denyIPVidRate! ", local_ip);
			alpaca_log_wirte(ALPACA_WARN, buf);
			//	curl = curl_easy_init();
			//	memset(url, 0, 100);
			//	sprintf(url, "%s:%d/hupeng?%s%s", "http://127.0.0.1", DEFAULT_ALARM_SERVICE_LISTEN_PORT, "long_time_no_update_denyIPVidRate!", local_ip);
			//	curl_easy_setopt(curl, CURLOPT_URL, url);
			//	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			//	curl_easy_perform(curl);
			//	curl_easy_cleanup(curl);

		}
		if(denyVisterIDRateExpire < (int)time(NULL)){
			curl = curl_easy_init();
			sprintf(url, "%s:%d/%s%s", "http://127.0.0.1", send_process_listen_port, "denyVisterIDRate", local_ip);	
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			char buf[100];
			ngx_memset(buf, 0, 100);
			sprintf(buf, "%s%s", "long time no update denyVisterIDRate! ", local_ip);
			alpaca_log_wirte(ALPACA_WARN, buf);
			//	curl = curl_easy_init();
			//	memset(url, 0, 100);
			//	sprintf(url, "%s:%d/hupeng?%s%s", "http://127.0.0.1", DEFAULT_ALARM_SERVICE_LISTEN_PORT, "long_time_no_update_denyVisterIDRate!", local_ip);
			//	curl_easy_setopt(curl, CURLOPT_URL, url);
			//	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			//	curl_easy_perform(curl);
			//	curl_easy_cleanup(curl);

		}
		if(acceptIPPrefixCount == 0 && first_time != 0){
			first_time ++;
			curl = curl_easy_init();
			sprintf(url, "%s:%d/%s", "http://127.0.0.1", send_process_listen_port, "acceptIPPrefix");	
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			curl_easy_perform(curl);
			curl_easy_cleanup(curl);

			char buf[100];
			ngx_memset(buf, 0, 100);
			sprintf(buf, "%s%s", "long time no update acceptIPPrefix! ", local_ip);
			alpaca_log_wirte(ALPACA_WARN, buf);
			//	curl = curl_easy_init();
			//	memset(url, 0, 100);
			//	sprintf(url, "%s:%d/hupeng?%s%s", "http://127.0.0.1", DEFAULT_ALARM_SERVICE_LISTEN_PORT, "long_time_no_update_acceptIPPrefix!", local_ip);
			//	curl_easy_setopt(curl, CURLOPT_URL, url);
			//	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
			//	curl_easy_perform(curl);
			//	curl_easy_cleanup(curl);
		}
		sleep(300);
	}	
}

void init(){
	initBlockRequestQueue();
	startPushRequestThread();
	startHealthCheckThread();
}

void startHealthCheckThread(){
	pthread_t tid;
	int err;
	void* (*ptr)(void *arg);
	ptr = healthCheckThread;
	err = pthread_create(&tid, NULL, ptr, NULL);
	if(err){
		alpaca_log_wirte(ALPACA_ERROR, "start health check thread fail");
	}
}

void startPushRequestThread(){
	pthread_t tid;
	int err;
	void* (*ptr)(void *arg);
	ptr = pushRequestThread;
	err = pthread_create(&tid, NULL, ptr, NULL);
	if(err){
		alpaca_log_wirte(ALPACA_ERROR, "start push request thread fail");
	}
}

httpParams_pool* multi_malloc_blockEvent(Context* context){
	int paramnum = PUSH_BLOCK_ARGS_NUM;
	alpaca_memory_pool* pool = alpaca_memory_pool_create(POOL_SIZE);
	Pair* httpParams = alpaca_memory_pool_malloc(pool, sizeof(Pair)*paramnum);
	if(!httpParams){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[0].key = alpaca_memory_pool_malloc(pool, strlen("blockUrl") + 1);
	if(!httpParams[0].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[0].value = alpaca_memory_pool_malloc(pool, context->rawUrl_len + 1);
	if(!httpParams[0].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[1].key = alpaca_memory_pool_malloc(pool,strlen("status") + 1);
	if(!httpParams[1].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[1].value = alpaca_memory_pool_malloc(pool, 4);
	if(!httpParams[1].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[2].key = alpaca_memory_pool_malloc(pool, strlen("blockIp") + 1);
	if(!httpParams[2].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	if(!context->clientIP){
		httpParams[2].value = alpaca_memory_pool_malloc(pool, strlen("empty ip") + 1);
		if(!httpParams[2].value){
			alpaca_memory_pool_destroy(pool);
			return NULL;
		}
	}
	else{
		httpParams[2].value = alpaca_memory_pool_malloc(pool, context->clientIP_len + 1);
		if(!httpParams[2].value){
			alpaca_memory_pool_destroy(pool);
			return NULL;
		}
	}
	httpParams[3].key = alpaca_memory_pool_malloc(pool, strlen("userAgent") + 1);
	if(!httpParams[3].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[3].value = alpaca_memory_pool_malloc(pool, context->userAgent_len + 1);
	if(!httpParams[3].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[4].key = alpaca_memory_pool_malloc(pool, strlen("httpMethod") + 1);
	if(!httpParams[4].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[4].value = alpaca_memory_pool_malloc(pool, context->httpMethod_len + 1);
	if(!httpParams[4].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[5].key = alpaca_memory_pool_malloc(pool, strlen("clientIP") + 1);
	if(!httpParams[5].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[5].value = alpaca_memory_pool_malloc(pool, strlen(local_ip) + 1);
	if(!httpParams[5].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[6].key = alpaca_memory_pool_malloc(pool, strlen("vid") + 1);
	if(!httpParams[6].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	if(!context->visitId){
		httpParams[6].value = alpaca_memory_pool_malloc(pool, strlen("empty ip") + 1);
		if(!httpParams[6].value){
			alpaca_memory_pool_destroy(pool);
			return NULL;
		}
	}
	else{
		httpParams[6].value = alpaca_memory_pool_malloc(pool, context->visitId_len + 1);
		if(!httpParams[6].value){
			alpaca_memory_pool_destroy(pool);
			return NULL;
		}
	}
	httpParams[7].key = alpaca_memory_pool_malloc(pool, strlen("logTime") + 1);
	if(!httpParams[7].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[7].value = alpaca_memory_pool_malloc(pool, strlen("yyyy-MM-dd HH:mm:ss") + 1);
	if(!httpParams[7].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[8].key = alpaca_memory_pool_malloc(pool, strlen(TOKEN_KEY) + 1);
	if(!httpParams[8].key){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams[8].value = alpaca_memory_pool_malloc(pool, MD5_LEN*sizeof(char));
	if(!httpParams[8].value){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	httpParams_pool* p = alpaca_memory_pool_malloc(pool, sizeof(httpParams_pool));
	if(!p){
		alpaca_memory_pool_destroy(pool);
		return NULL;
	}
	p->httpParams = httpParams;
	p->pool = pool;
	return p;
}

int doFilter(ngx_http_request_t *r, ngx_chain_t **out){
	Context* context = NULL;
	if(switchconfig->mount == 1){
		context = getRequestContext(r);
		if(context == NULL){
			alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get context of a request");
			return CONTEXTSTATUSNEEDNOTRESPONSE;
		}
		procrequest(r, context);
		if(responseIfNeeded(r, context, out) == CONTEXTSTATUSNEEDRESPONSE){
			if(switchconfig->pushBlockEvent == 1){
				httpParams_pool* p = multi_malloc_blockEvent(context);
				if(!p){
					alpaca_log_wirte(ALPACA_WARN, "malloc fail, when create memory pool for block event info");
					return CONTEXTSTATUSNEEDRESPONSE;
				}
				strcpy(p->httpParams[0].key, "blockUrl");
				strncpy(p->httpParams[0].value, (char*)context->rawUrl, context->rawUrl_len);
				strcpy(p->httpParams[1].key, "status");
				char* httpstatus = getHttpStatus(p->pool, context->status);
				if(!httpstatus){
					httpParams_pool_free(p);
					return CONTEXTSTATUSNEEDRESPONSE;
				}
				strcpy(p->httpParams[1].value, httpstatus);
				strcpy(p->httpParams[2].key, "blockIp");
				if(!context->clientIP){
					strcpy(p->httpParams[2].value, "empty ip");
				}
				else{
					strncpy(p->httpParams[2].value, (char*)context->clientIP, context->clientIP_len);
				}
				strcpy(p->httpParams[3].key, "userAgent");
				strncpy(p->httpParams[3].value, (char*)context->userAgent, context->userAgent_len);
				strcpy(p->httpParams[4].key, "httpMethod");
				strncpy(p->httpParams[4].value, (char*)context->httpMethod, context->httpMethod_len);
				strcpy(p->httpParams[5].key, "clientIP");
				strcpy(p->httpParams[5].value, local_ip);
				strcpy(p->httpParams[6].key, "vid");
				if(!context->visitId){
					strcpy(p->httpParams[6].value, "empty id");
				}
				else{
					strncpy(p->httpParams[6].value, (char*)context->visitId, context->visitId_len);
				}
				strcpy(p->httpParams[7].key, "logTime");
				getNowLogTime(p->httpParams[7].value);
				blockQueueOffer(p);
			}
			return CONTEXTSTATUSNEEDRESPONSE;
		}
		else{
			return CONTEXTSTATUSNEEDNOTRESPONSE;
		}
	}
	return CONTEXTSTATUSNEEDNOTRESPONSE;
}

void changeIntToChar(char** buf, int num, int bit){
	while(bit > 0){
		sprintf(*buf, "%d", num / bit);
		(*buf)++;
		num = num % bit;
		bit = bit / 10;
	}
}

char* getNowLogTime(char* result){
	time_t t;
	struct tm *local;
	time(&t);
	local = localtime(&t);
	int date[6];
	date[0] = local->tm_year + 1900;
	date[1] = local->tm_mon + 1;
	date[2] = local->tm_mday;
	date[3] = local->tm_hour;
	date[4] = local->tm_min;
	date[5] = local->tm_sec;
	char* buf = result;
	changeIntToChar(&buf, date[0], 1000);
	sprintf(buf, "%s", "-");
	buf++;
	changeIntToChar(&buf, date[1], 10);
	sprintf(buf, "%s", "-");
	buf++;
	changeIntToChar(&buf, date[2], 10);
	sprintf(buf, "%s", " ");
	buf++;
	changeIntToChar(&buf, date[3], 10);
	sprintf(buf, "%s", ":");
	buf++;
	changeIntToChar(&buf, date[4], 10);
	sprintf(buf, "%s", ":");
	buf++;
	changeIntToChar(&buf, date[5], 10);
	return result;
}

void procrequest(ngx_http_request_t *r, Context *context){
	context->status = SUCCESS;
	if(handleInternalRequestIfNeeded(r, context) == 0){
		return;
	}
	handleBlockRequestIfNeeded(context, r);	
}

int handleInternalRequestIfNeeded(ngx_http_request_t *r, Context *context){
	if(isFirewallRequest(r)){
		if(strncmp((char*)context->rawUrl, commonconfig->clientStatusUrl, context->rawUrl_len) == 0){
			context->status = SHOWSTATUS;
			return 0;
		}
		else if(strncmp((char*)context->rawUrl, commonconfig->clientEnableUrl, context->rawUrl_len) == 0){
			switchconfig->enable = 1;  
			set_string("true", &switchconfig->string_enable);
			context->status = SHOWSTATUS;
			return 0;
		}
		else if(strncmp((char*)context->rawUrl, commonconfig->clientDisableUrl, context->rawUrl_len) == 0){
			switchconfig->enable = 0;
			set_string("false", &switchconfig->string_enable);
			context->status = SHOWSTATUS;
			return 0;
		}
	}
	return -1;
}

int isFirewallRequest(ngx_http_request_t *r){
	if(r != NULL && strncasecmp((char*)r->method_name.data, "POST", r->method_name.len) == 0){
		if(r->header_name_start){
			char* token = strstr((char*)r->header_name_start, TOKEN_KEY);
			if(!token){
				return 0;
			}
			token = token + strlen(TOKEN_KEY) + 1;
			char* token_compute = ngx_pcalloc(r->pool, r->connection->addr_text.len + r->unparsed_uri.len + 2);
			if(!token_compute){
				return 0;
			}
			strncpy(token_compute, (char*)r->unparsed_uri.data, r->unparsed_uri.len);
			strcat(token_compute, "|");
			strcat(token_compute, local_ip);
			char* md5_token_compute = getmd5fromngxpool(r, token_compute);
			if(!md5_token_compute){
				return 0;
			}
			if(strncmp(md5_token_compute, token, 32) == 0){
				return 1;
			}
			else{
				return 0;
			}
		}
		else{
			return 0;
		}
		return 1;		
	}
	return 0;	
}

int getHttpParam(u_char** in, ngx_http_request_t *r){
	if(!visitId){
		*in = NULL;
		return 0;
	}
	if(r->headers_in.cookies.nelts == 0){
		*in = NULL;
		//return 0;
	}
	ngx_table_elt_t** cookies = r->headers_in.cookies.elts;
	int i = 0;
	for(i = 0; i < (int)r->headers_in.cookies.nelts; i++){

		*in = (u_char*)strstr((char*)(cookies[i])->value.data, visitId);
		if(*in == NULL){
			continue;
		}
		u_char* end = NULL;
		end = (u_char*)strstr((char*)(*in), "; ");
		if((U_CHAR)end != -1 && end){
			u_char* end_tmp = end + 2;
			while(end_tmp < cookies[i]->value.data + cookies[i]->value.len){
				if(*end_tmp != ' '){
					break;
				}
				end_tmp ++;
			}
			if(end_tmp < cookies[i]->value.data + cookies[i]->value.len){
				end = end -1;
			}
		}
		else{
			end = (u_char*)(cookies[i])->value.data + (cookies[i])->value.len - 1;
		}
		*in = *in + strlen(visitId) + 1;
		if(strncmp((char*)*in, "\"", 1) == 0){
			(*in)++;
		}
		if(strncmp((char*)end, "\"", 1) == 0){
			end--;
		}
		if(strncmp((char*)*in, "\"", 1) == 0){
			return 0;
		}
		else{
			if(strncmp((char*)*in, "\\", 1) == 0 && strncmp((char*)(*in + 1), "\"", 1) == 0){
				(*in) = (*in) + 2;
			}
			if(strncmp((char*)(end - 1), "\\", 1) == 0 && strncmp((char*)(end), "\"", 1) == 0){
				end = end - 2;
			}
		}
		return (end - (*in) + 1);
	}
	for(i = 0; i < (int)r->headers_in.headers.part.nelts; i ++){
		if(strlen(visitId) != (unsigned int)(((ngx_table_elt_t*)r->headers_in.headers.part.elts + i)->key.len)){
			continue;
		}
		if(strncmp(visitId, (char*)((ngx_table_elt_t*)r->headers_in.headers.part.elts + i)->key.data, strlen(visitId)) != 0){
			continue;
		}
		*in = ((ngx_table_elt_t*)r->headers_in.headers.part.elts + i)->value.data;
		return ((ngx_table_elt_t*)r->headers_in.headers.part.elts + i)->value.len;
	}
	*in = NULL;
	return 0;
}

char* find_client_ip(char* buf){
	char* tok1 = strsep(&buf, ", ");
	char* result;
	if(buf){
		result = find_client_ip(buf);
		if(result){
			return result;
		}
	}
	if(strlen(tok1) == 0 || strncmp(tok1, "10.", 3) == 0 || strncmp(tok1, "192.168.", 8) == 0 || strncmp(tok1, "127.", 4) == 0){
		return NULL;
	}
	else{
		return tok1;
	}

}

u_char* get_client_ip(ngx_http_request_t *r, size_t* len){
	u_char* result;
	if(strncmp((char*)r->connection->addr_text.data, "10.", 3) == 0 || strncmp((char*)r->connection->addr_text.data, "192.168.", 8) == 0 || strncmp((char*)r->connection->addr_text.data, "127.", 4) == 0){
		if(r->headers_in.x_forwarded_for){
			if(r->headers_in.x_forwarded_for->value.data){
				char* clientIP = ngx_pcalloc(r->pool, r->headers_in.x_forwarded_for->value.len + 1);
				if(!clientIP){
					result = (u_char*)find_client_ip((char*)r->headers_in.x_forwarded_for->value.data);
				}
				else{
					strcpy(clientIP, (char*) r->headers_in.x_forwarded_for->value.data);
					result = (u_char*)find_client_ip(clientIP);
				}
				if(result){
					*len = strlen((char*)result);
					return result;
				}
			}
		}
	}
	*len = r->connection->addr_text.len;
	return r->connection->addr_text.data;
}

Context* getRequestContext(ngx_http_request_t *r){
	Context* result = ngx_pcalloc(r->pool, sizeof(Context));
	if(result == NULL){
		return NULL;
	}
	if(r->headers_in.user_agent){
		result->userAgent = r->headers_in.user_agent->value.data;
		result->userAgent_len = r->headers_in.user_agent->value.len;
	}
	else{
		result->userAgent = NULL;
		result->userAgent = 0;
	}
	result->httpMethod = r->method_name.data;
	result->httpMethod_len = r->method_name.len;
	result->clientIP = get_client_ip(r, &result->clientIP_len);
	result->rawUrl = r->unparsed_uri.data;
	result->rawUrl_len = r->unparsed_uri.len;
	result->visitId_len = getHttpParam(&result->visitId, r);
	if(r->headers_in.host){
		result->domain = r->headers_in.host->value.data;
		result->domain_len = r->headers_in.host->value.len;
	}
	else{
		result->domain_len = 0;
	}
	return result;
}

int is_empty_string(char* buf, int buflen){
	int i;
	if(buflen == 0){
		return 1;
	}
	for(i = 0; i < buflen; i++){
		if(buf[i] != ' ' || buf[i] != '\t' || buf[i] != '\n'){
			return 0;
		}
	}
	return 1;	
}

void handleBlockRequestIfNeeded(Context *context, ngx_http_request_t *r){
	if(switchconfig->enable == 1){
		lua_getglobal(L,"block");              
		lua_pushlstring(L, (char*)context->clientIP, context->clientIP_len);
		if(context->userAgent_len == 0){
			lua_pushlstring(L, " ", 1);
		}
		else{
			lua_pushlstring(L, (char*)context->userAgent, context->userAgent_len);
		}
		lua_pushlstring(L, (char*)context->httpMethod, context->httpMethod_len);
		lua_pushlstring(L, (char*)context->rawUrl, context->rawUrl_len);
		if(context->visitId_len == 0){
			lua_pushlstring(L, " ", 1);
		}
		else{
			lua_pushlstring(L, (char*)context->visitId, context->visitId_len);
		}
		if(context->domain_len == 0){
			lua_pushlstring(L, " ", 1);
		}
		else{
			lua_pushlstring(L, (char*)context->domain, context->domain_len);
		}
		lua_pcall(L,6,1,0);
		int judge = lua_tonumber(L, 1);
		ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "hupeng test block number %d", judge);
		lua_pop(L, 1);
		if(judge == 0){       
			lua_close(L);                                                          
			L = luaL_newstate();                                               
			luaL_openlibs(L);                                                 
			luaL_loadfile(L,lua_filename);                                   
			lua_pcall(L,0,0,0);                                             
		}
		context->status = judge;
	}

}

int compareDate(char* forbidDate){
	if(!forbidDate){
		return -1;
	}
	int len = strlen(forbidDate) - 2;
	int i;
	int j = 0;
	int num = 0;
	time_t t;
	struct tm *local;
	time(&t);
	local = localtime(&t);
	int date[6];
	date[0] = local->tm_year + 1900;
	date[1] = local->tm_mon + 1;
	date[2] = local->tm_mday;
	date[3] = local->tm_hour;
	date[4] = local->tm_min;
	date[5] = local->tm_sec;
	for(i = 0; i < len; i++){
		if(isdigit(forbidDate[i + 1])){
			num = num * 10 + atoi(&forbidDate[i]);
		}
		else{
			if(num > date[j]){
				return 1;
			}
			num = 0;
			j++;
			if(j >= 6){
				return -1;
			}
		}
	}
	return -1;
}

int responseIfNeeded(ngx_http_request_t *r, Context *context, ngx_chain_t **out){
	if(context == NULL){
		return CONTEXTSTATUSNEEDNOTRESPONSE;
	}
	switch(context->status){
		case SHOWSTATUS:
			responseStatus(r, out);
			return CONTEXTSTATUSNEEDRESPONSE;
		case DENY_HTTPMETHOD:
		case DENY_IP:
		case DENY_USERAGENT:
		case DENY_NOVID:
		case DENY_VID:
			responseDenyMessage(r, context, out);
			return CONTEXTSTATUSNEEDRESPONSE;
		case DENY_IPRATE:
		case DENY_IPVIDRATE:
		case DENY_VIDRATE:
			responseDenyRateMessage(r, context, out);
			return CONTEXTSTATUSNEEDRESPONSE;
		default:
			break;	
	}
	return CONTEXTSTATUSNEEDNOTRESPONSE;
}

void responseStatus(ngx_http_request_t *r, ngx_chain_t **out){
	char* alpacastatus = dumpStatus();
	ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "%d\n%s", strlen(alpacastatus),  alpacastatus);
	ngx_buf_t    *b;  
	b = ngx_calloc_buf(r->pool);  
	if (b == NULL) {
		return;  
	} 
	char *res = ngx_pcalloc(r->pool, strlen(alpacastatus) + 1);
	strcpy(res, alpacastatus);
	free(alpacastatus);
	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_length_n = strlen(res);
	b->pos = (u_char *) res;
	b->last = b->pos + strlen(res);  
	b->memory = 1;  //TODO
	b->last_buf = 1;  
	(*out) = ngx_alloc_chain_link(r->pool);  
	if (*out == NULL){
		return;
	}	
	(*out)->buf = b;  
	(*out)->next = NULL;  
	(*out)->buf->last_buf = 1;  
}

void responseDenyMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out){
	char* resbody = getResponseDenyMessage(r, context);	
	ngx_buf_t    *b;  
	b = ngx_calloc_buf(r->pool);  
	if (b == NULL) {
		return;  
	} 
	if(resbody == NULL){
		return;
	}
	r->headers_out.status = NGX_HTTP_FORBIDDEN;
	r->headers_out.content_length_n = strlen(resbody);
	b->pos = (u_char *) resbody;
	b->last = b->pos + strlen(resbody);  
	b->memory = 1;  
	b->last_buf = 1;  
	(*out) = ngx_alloc_chain_link(r->pool);  
	if (*out == NULL){
		return;
	}	
	(*out)->buf = b;  
	(*out)->next = NULL;  
	(*out)->buf->last_buf = 1;  
}

void responseDenyRateMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out){
	char* resbody = getResponseDenyRateMessage(r, context);	
	ngx_buf_t    *b;  
	b = ngx_calloc_buf(r->pool);  
	if (b == NULL) {
		return;  
	} 
	if(resbody == NULL){
		return;
	}
	r->headers_out.status = NGX_HTTP_FORBIDDEN;
	r->headers_out.content_length_n = strlen(resbody);
	b->pos = (u_char *) resbody;
	b->last = b->pos + strlen(resbody);  
	b->memory = 1;  
	b->last_buf = 1;  
	*out = ngx_alloc_chain_link(r->pool);  
	if (*out == NULL){
		return;
	}	
	(*out)->buf = b;  
	(*out)->next = NULL;  
	(*out)->buf->last_buf = 1;  
}



char* getResponseDenyMessage(ngx_http_request_t *r, Context *context){
	char* result = ngx_pcalloc(r->pool, DENYMESSAGEMAXLENTH);
	if(result == NULL){
		return NULL;
	}
	int denymessage_len = strlen(responsemessageconfig->denyMessage);
	int i, j, k;
	k = 0;
	for(i = 0; i < denymessage_len; i++){
		if(responsemessageconfig->denyMessage[i] == '$' && responsemessageconfig->denyMessage[i + 1] == '{'){
			j = i + 2;
			int num = 0;
			while(j < denymessage_len && responsemessageconfig->denyMessage[j] != '}'){
				num = num * 10 + atoi(&responsemessageconfig->denyMessage[j]);
				j++;
			}
			if(num == 1){
				char buf[10];
				ngx_memset(buf, 0, 10);
				sprintf(buf, "%d", context->status);
				int m = 0;
				while(m < 3){
					result[k] = buf[m];
					k++;
					m++;
				}
			}
			else if(num == 2){
				int len = context->clientIP_len;
				int m = 0;
				while(m < len){
					result[k] = context->clientIP[m];
					k++;
					m++;
				}
			}
			else if(num == 3){
				int len = strlen((char*)context->userAgent);
				int m = 0;
				while(m < len){
					result[k] = context->userAgent[m];
					k++;
					m++;
				}
			}
			i = j;
		}
		else{
			result[k] = responsemessageconfig->denyMessage[i];
			k++;
		}
	}
	return result;
}

char* getResponseDenyRateMessage(ngx_http_request_t *r, Context *context){
	char* result = ngx_pcalloc(r->pool, DENYMESSAGEMAXLENTH);
	if(result == NULL){
		return NULL;
	}
	int denymessage_len = strlen(responsemessageconfig->denyRateMessage);
	int i, j, k;
	k = 0;
	for(i = 0; i < denymessage_len; i++){
		if(responsemessageconfig->denyRateMessage[i] == '$' && responsemessageconfig->denyRateMessage[i + 1] == '{'){
			j = i + 2;
			int num = 0;
			while(j < denymessage_len && responsemessageconfig->denyRateMessage[j] != '}'){
				num = num * 10 + atoi(&responsemessageconfig->denyRateMessage[j]);
				j++;
			}
			if(num == 0){
				int len = context->clientIP_len;
				int m = 0;
				while(m < len){
					result[k] = context->clientIP[m];
					k++;
					m++;
				}
			}
			i = j;
		}
		else{
			result[k] = responsemessageconfig->denyRateMessage[i];
			k++;
		}
	}
	return result;
}
