
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
#include <pthread.h>
#include <curl/curl.h>
#include <time.h>

#include "switchconfig.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "cJSON.h"
#include "alpacaClient.h"
#include "collectionUtils.h"
#include "urlencode.h"
#include "blockrequestqueue.h"
#include "md5.h"
#include "alpaca_log.h"

#define ZOOKEEPERBUFSIZE 20480
#define ZOOKEEPERROUTE "/DP/CONFIG/"
#define DENYMESSAGEMAXLENTH 4096
#define DEFAULT_BLOCK_MAX_LENTH 4096
#define DEFAULT_HEARTBEAT_MAX_LENTH 8192
#define TOKEN_KEY "alpaca-firewall-token"
#define DEFAULT_SERVERROOT "http://192.168.26.38:8888"
#define DEFAULT_SERVER_URL_BLOCK_EVENT "/clientManagement/dianping.firewall.server.blockevent"
#define DEFAULTDENYRATE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#ccc;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}form{{padding:8px;font-size:14px;line-height:18px;text-align:center;}}form input{{font-size:20px;font-weight:bold;}}form input.i{{width:190px;}}p{{margin-bottom:30px;}}div{{margin-bottom:8px;}}p.c{{color:#ccc;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><form method=\"post\" action=\"/validcode\"><p>对不起，你访问的太快了，请输入验证码后继续浏览：</p><div><img  id=\"code\" src=\"/deny.code\" alt=\"验证码\" /></div><div> <input name=\"vode\" class=\"i\" type=\"text\" /><input type=\"submit\" value=\" 提 交 \" /><input type=\"hidden\" name=\"referer\" value=\"hupeng\" /></div><p class=\"c\">如果您(${0})经常碰到此情况，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，我们会尽快处理。</p></form><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_rate\");</script></body></html>"
#define DEFAULTDENYMESSAGE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#2583ad;text-decoration:none;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}p{{text-align:center;}}sub{{display:block;margin:0;padding:0;color:#aaa;font-size:11px;text-align:right;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><p>对不起，您的访问存在某些问题。<br />如果您是正常访问，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，并附上以下信息：<br /><textarea rows=\"10\" cols=\"80\">${0}\r\n${1}\r\n${2}</textarea></p><sub>${0}</sub><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_agent\");</script></body></html>"
#define DEFAULT_CLIENT_URL_DISABLE "/dianping.firewall.client.disable"
#define DEFAULT_CLIENT_URL_ENABLE "/dianping.firewall.client.enable"
#define DEFAULT_CLIENT_HEARTBEAT_ENABLE 0
#define DEFAULT_CLIENT_HEARTBEAT_INTERVAL 180
#define DEFAULT_CLIENT_URL_STATUS "/dianping.firewall.client.status"
#define DEFAULT_CLIENT_URL_VALIDATECODE "/deny.code"
#define ALPACA_CLIENT_VERSION "0.3.5"
#define DEFAULT_SERVER_URL_HEARTBEAT "/clientManagement/dianping.firewall.server.heartbeat"
#define EXPIRETIME 180
#define POOL_SIZE 1024*10
#ifdef __x86_64__
#define U_CHAR long
#elif __i386__
#define U_CHAR int
#endif


static int heartbeatthreadstart;
static int pushblockthreadstart;
static char* visitId;
static char* alpaca_log_file;
static alpaca_log_t alpaca_log;
static zhandle_t *zh;
static char* local_ip;
static time_t expiretime;
static char* zookeeper_key[] = {"alpaca.filter.enable", "alpaca.policy.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.acceptIPPrefix", "alpaca.policy.acceptHttpMethod", "alpaca.policy.denyUserAgent", "alpaca.policy.denyUserAgentPrefix", "alpaca.policy.denyIPAddressPrefix", "alpaca.policy.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.denyIPVidRate", "alpaca.policy.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.denyVisterID", "alpaca.policy.denyVisterIDRate"}; 

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf, char *key);
void getLocalIP();
void setDefault();
char* getCharPInstance(char* buf);
int setCharP(char* buf, char** key);
int* getIntPInstanceDigit(char* buf);
int setIntPDigit(char* buf, int** key);
int getIntInstance(char* buf);
int setInt(char* buf, int* key);
List* getListPInstance(char* buf);
int setListP(char* buf, List** key);
PairList* getPairListPInstance(char* buf);
int setPairListP(char* buf, PairList** key);
TripleList* getTripleListPInstance(char* buf);
int setTripleListP(char* buf, TripleList** key);
ListList* getListListPInstance(char *buf);
int setListListP(char* buf, ListList** key);
void procrequest(ngx_http_request_t *r, Context *context);
int handleInternalRequestIfNeeded(ngx_http_request_t *r, Context *context);
int isFirewallRequest(ngx_http_request_t *r);
Context* getRequestContext(ngx_http_request_t *r);
void handleBlockRequestIfNeeded(Context *context);
int compareDate(char* forbidDate);
int responseIfNeeded(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
void responseStatus(ngx_http_request_t *r, ngx_chain_t **out);
void responseDenyMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
void responseDenyRateMessage(ngx_http_request_t *r, Context *context, ngx_chain_t **out);
cJSON* dumpStatus();
cJSON* formatCharPP(char** key, int key_len);
cJSON* formatPairPP(Pair* key, int key_len);
cJSON* formatListPP(List* key, int key_len);
char* getResponseDenyMessage(ngx_http_request_t *r, Context *context);
char* getResponseDenyRateMessage(ngx_http_request_t *r, Context *context);
char* getNowLogTime(char* result);

char* getHttpStatus(alpaca_memory_pool* pool, enum status s){
	char* buf = alpaca_memory_poll_malloc(pool, sizeof(char) * 4);
	if(!buf){
		return NULL;
	}
	int compute = (int)s;
	sprintf(buf, "%d%d%d", compute/100, (compute/10)%10, compute%100);
	return buf;
}
void initBlockRequestQueue(){
	blockRequestQueue.head = 0;
	blockRequestQueue.tail = 0;
	blockRequestQueue.size = BLOCKREQUESTQUEUESIZE + 1;
}

int PairUrlEncode(Pair* httpParams, char* out, int len){
	int isFirst = 1;
	int p = 0;
	int new_length = 0;
	char* buf;
	int i;
	for(i = 0; i < len; i++){
		if(isFirst == 1){
			isFirst = 0;
		}
		else{
			out[p] = '&';
			p++;
		}
		buf = url_encode(httpParams[i].key, strlen(httpParams[i].key), &new_length);
		if(!buf){
			return -1;
		}
		strcat(out, buf);
		free(buf);
		p++;
		p = p + new_length;
		strcat(out,"=");
		buf = url_encode(httpParams[i].value, strlen(httpParams[i].value), &new_length);	
		if(!buf){
			return -1;
		}
		strcat(out,buf);
		free(buf);
		p = p + new_length;
	}	
	return 1;
}

void sendFirewallHttpRequest(){
	httpParams_pool* httpParams = blockQueuePoll();
	if(!httpParams){
		return;
	}
	CURL *curl;
	char *reqUrl = alpaca_memory_poll_malloc(httpParams->pool, strlen(commonconfig.serverRoot) + strlen(commonconfig.serverBlockEventUrl) + 1);
	if(!reqUrl){
		return;
	}
	char *out = alpaca_memory_poll_malloc(httpParams->pool, DEFAULT_BLOCK_MAX_LENTH);
	if(!out){
		return;
	}
	strcpy(httpParams->httpParams[8].key, TOKEN_KEY);
	char* urlbuf = alpaca_memory_poll_malloc(httpParams->pool, strlen(commonconfig.serverBlockEventUrl) + strlen(local_ip) + 2);
	if(!urlbuf){
		return;
	}
	strcpy(urlbuf, commonconfig.serverBlockEventUrl);
	strcat(urlbuf, "|");
	strcat(urlbuf, local_ip);
	getmd5frompool(httpParams->httpParams[8].value, urlbuf);
	strcat(httpParams->httpParams[8].value, "\0");
	if(PairUrlEncode(httpParams->httpParams, out, PUSH_BLOCK_ARGS_NUM) == -1){
		return;
	}
	//strcpy(out, "hupeng+++++++++++++++++++++++++");
	strcpy(reqUrl, commonconfig.serverRoot);
	strcat(reqUrl, commonconfig.serverBlockEventUrl);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, reqUrl);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, out);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	//freePairP(httpParams, PUSH_BLOCK_ARGS_NUM);

	if(time(NULL) > expiretime){
		while(freelist){
			httpParams_pool_list* freelist_head;
			do{
				freelist_head = freelist;
			}while(!__sync_bool_compare_and_swap(&freelist, freelist_head, freelist_head->next));
			alpaca_memory_poll_destroy(freelist_head->value->pool);		
		}
		expiretime = time(NULL) + EXPIRETIME;
	}
}


httpParams_pool* multi_malloc_heartbeatRequest(int paramnum){
	alpaca_memory_pool* p = alpaca_memory_pool_create(POOL_SIZE);
	if(!p){
		return NULL;
	}
	Pair* httpParams = alpaca_memory_poll_malloc(p, sizeof(Pair)*paramnum);
	httpParams[0].key = alpaca_memory_poll_malloc(p, strlen("clientIP") + 1);
	if(!httpParams[0].key){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[0].value = alpaca_memory_poll_malloc(p, strlen(local_ip) + 1);
	if(!httpParams[0].value){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[1].key = alpaca_memory_poll_malloc(p, strlen("version") + 1);
	if(!httpParams[1].key){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[1].value = alpaca_memory_poll_malloc(p, strlen(ALPACA_CLIENT_VERSION) + 1);
	if(!httpParams[1].value){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[2].key = alpaca_memory_poll_malloc(p, strlen("enable") + 1);
	if(!httpParams[2].key){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[2].value = alpaca_memory_poll_malloc(p, 6);
	if(!httpParams[2].value){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[3].key = alpaca_memory_poll_malloc(p, strlen(TOKEN_KEY) + 1);
	if(!httpParams[3].key){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams[3].value = alpaca_memory_poll_malloc(p, MD5_LEN*sizeof(char));
	if(!httpParams[3].value){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	httpParams_pool* pool = alpaca_memory_poll_malloc(p, sizeof(httpParams_pool));
	if(!pool){
		alpaca_memory_poll_destroy(p);
		return NULL;
	}
	pool->httpParams = httpParams;
	pool->pool = p;
	return pool;
}

void httpParams_pool_free(httpParams_pool* p){
	alpaca_memory_poll_destroy(p->pool);
}

void sendFirewallHeartbeatRequest(){
	int paramnum = 4;
	httpParams_pool* p =  multi_malloc_heartbeatRequest(paramnum);
	CURL *curl;
	char *reqUrl = alpaca_memory_poll_malloc(p->pool, strlen(commonconfig.serverRoot) + strlen(commonconfig.serverHeartbeatUrl) + 1);
	if(!reqUrl){
		httpParams_pool_free(p);
		return;
	}
	char *out = alpaca_memory_poll_malloc(p->pool, DEFAULT_HEARTBEAT_MAX_LENTH);
	if(!out){
		httpParams_pool_free(p);
		return;
	}
	strcpy(p->httpParams[0].key, "clientIP");
	strcpy(p->httpParams[0].value, local_ip);
	strcpy(p->httpParams[1].key, "version");
	strcpy(p->httpParams[1].value, ALPACA_CLIENT_VERSION);
	strcpy(p->httpParams[2].key, "enable");
	if(switchconfig.enable){
		strcpy(p->httpParams[2].value, "true");
	}
	else{
		strcpy(p->httpParams[2].value, "false");
	}
	strcpy(p->httpParams[3].key, TOKEN_KEY);
	char* urlbuf = alpaca_memory_poll_malloc(p->pool, strlen(commonconfig.serverHeartbeatUrl) + strlen(local_ip) + 2);
	if(!urlbuf){
		httpParams_pool_free(p);
		return;
	}
	strcpy(urlbuf, commonconfig.serverHeartbeatUrl);
	strcat(urlbuf, "|");
	strcat(urlbuf, local_ip);
	getmd5frompool(p->httpParams[3].value, urlbuf);
	strcat(p->httpParams[3].value, "\0");

	if(PairUrlEncode(p->httpParams, out, paramnum) == -1){
		httpParams_pool_free(p);
		return;
	}
	//strcpy(out, "hupeng+++++++++++++++++++++++++");
	strcpy(reqUrl, commonconfig.serverRoot);
	strcat(reqUrl, commonconfig.serverHeartbeatUrl);
	/*alpaca_memory_pool* p = alpaca_memory_pool_create(POOL_SIZE);
	  if(!p){
	  return;
	  }
	  CURL *curl;
	  char *reqUrl = alpaca_memory_poll_malloc(p, strlen(commonconfig.serverRoot) + strlen(commonconfig.serverHeartbeatUrl) + 1);
	  if(!reqUrl){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  char *out = alpaca_memory_poll_malloc(p, DEFAULT_HEARTBEAT_MAX_LENTH);
	  if(!out){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  int paramnum = 4;
	  Pair* httpParams = alpaca_memory_poll_malloc(p, sizeof(Pair)*paramnum);
	  httpParams[0].key = alpaca_memory_poll_malloc(p, strlen("clientIP") + 1);
	  if(!httpParams[0].key){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[0].key, "clientIP");
	  httpParams[0].value = alpaca_memory_poll_malloc(p, strlen(local_ip) + 1);
	  if(!httpParams[0].value){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[0].value, local_ip);
	  httpParams[1].key = alpaca_memory_poll_malloc(p, strlen("version") + 1);
	  if(!httpParams[1].key){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[1].key, "version");
	  httpParams[1].value = alpaca_memory_poll_malloc(p, strlen(ALPACA_CLIENT_VERSION) + 1);
	  if(!httpParams[1].value){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[1].value, ALPACA_CLIENT_VERSION);
	  httpParams[2].key = alpaca_memory_poll_malloc(p, strlen("enable") + 1);
	  if(!httpParams[2].key){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[2].key, "enable");
	  if(switchconfig.enable){
	  httpParams[2].value = alpaca_memory_poll_malloc(p, strlen("true") + 1);
	  if(!httpParams[2].value){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[2].value, "true");
	  }
	  else{
	  httpParams[2].value = alpaca_memory_poll_malloc(p, strlen("false") + 1);
	  if(!httpParams[2].value){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[2].value, "false");
	  }
	  httpParams[3].key = alpaca_memory_poll_malloc(p, strlen(TOKEN_KEY) + 1);
	  if(!httpParams[3].key){
	  alpaca_memory_poll_destroy(p);
	  return;
	  }
	  strcpy(httpParams[3].key, TOKEN_KEY);
	  char* urlbuf = alpaca_memory_poll_malloc(p, strlen(commonconfig.serverHeartbeatUrl) + strlen(local_ip) + 2);
	  if(!urlbuf){
	alpaca_memory_poll_destroy(p);
	return;
}
strcpy(urlbuf, commonconfig.serverHeartbeatUrl);
strcat(urlbuf, "|");
strcat(urlbuf, local_ip);
httpParams[3].value = getmd5frompool(p, urlbuf);
strcat(httpParams[3].value, "\0");

if(PairUrlEncode(httpParams, out, paramnum) == -1){
	alpaca_memory_poll_destroy(p);
	return;
}
//strcpy(out, "hupeng+++++++++++++++++++++++++");
strcpy(reqUrl, commonconfig.serverRoot);
strcat(reqUrl, commonconfig.serverHeartbeatUrl);*/
curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, reqUrl);
//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
curl_easy_setopt(curl, CURLOPT_POST, 1);
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, out);
curl_easy_perform(curl);
curl_easy_cleanup(curl);
httpParams_pool_free(p);
}

void* pushRequestThread(){
	while(1) {
		int needSleep = 1;
		if(isBlockQueueEmpty() == 0){
			needSleep = 0;
			sendFirewallHttpRequest();
			usleep(5000);//TODO  5ms
		}
		if(needSleep){
			sleep(1);
		}
	}
	return NULL;
}

void startPushRequestThread(ngx_http_request_t *r){
	if(!pushblockthreadstart){
		pushblockthreadstart = 1;
		pthread_t tid;
		tid = pthread_create(&tid, NULL, pushRequestThread, NULL);
		if(tid){
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"start push request thread, error num is \"%d\" ",
					tid);
		}
	}
}


void* heartbeatThread(){
	while(1) {
		if(switchconfig.clientHeartbeatEnable){
			sendFirewallHeartbeatRequest();
		}	
		sleep(*(commonconfig.clientHeartbeatInterval));
	}
	return NULL;
}

void startHeartbeatThread(ngx_http_request_t *r){
	if(!heartbeatthreadstart){
		heartbeatthreadstart = 1;
		pthread_t tid;
		tid = pthread_create(&tid, NULL, heartbeatThread, NULL);
		if(tid){
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
					"start heart beat thread, error num is \"%d\" ",
					tid);
		}
	}
}

void getVisitId(ngx_alpaca_client_main_conf_t *aclc){
	visitId = malloc(aclc->visitId.len + 1);
		alpaca_log_append(alpaca_log.fd, "malloc fail, when get visitId");	
	if(visitId == NULL){
		return;
	}
	memset(visitId, 0, aclc->visitId.len + 1);
	strcpy(visitId, (char*)aclc->visitId.data);
}

int get_alpaca_log_level(char* level){
	if(strcasecmp(level, "DEBUG") == 0){
		return 0;
	}
	else if(strcasecmp(level, "INFO") == 0){
		return 1;
	}
	else if(strcasecmp(level, "WARN") == 0){
		return 2;
	}
	else if(strcasecmp(level, "ERROR") == 0){
		return 3;
	}
	else{
		return 2;
	}
}

void openAlpacaLog(ngx_alpaca_client_main_conf_t *aclc){
	if(aclc->log.len != 0){
		alpaca_log_file = malloc(aclc->log.len + 1);
		if(!alpaca_log_file){
			alpaca_log_file = NULL;
			return;
		}
		memset(alpaca_log_file, 0, aclc->log.len + 1);
		strcpy(alpaca_log_file, (char*)aclc->log.data);
		alpaca_log.fd = alpaca_log_open(alpaca_log_file);
		if(!aclc->level.data){
			alpaca_log.log_level = DEFAULT_ALPACA_LOG_LEVEL;	
		}
		else{
			alpaca_log.log_level = get_alpaca_log_level((char*)aclc->level.data);
		}
		return;
	}
	alpaca_log_file = NULL;
}

void init(ngx_alpaca_client_main_conf_t *aclc, ngx_http_request_t *r){
	openAlpacaLog(aclc);
	getVisitId(aclc);
	getLocalIP();
	initConfigWatch(aclc, r);
	initBlockRequestQueue();
	startPushRequestThread(r);//TODO ensure start thread only once
	startHeartbeatThread(r);
}

void getLocalIP(){
	int fd;
	struct ifreq ifr;
	struct sockaddr_in* sin;
	char *ip;
	ip = (char*)malloc(32);
	if(!ip){
		return;
	}
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	memset(&ifr, 0x00, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	sin = (struct sockaddr_in* )&ifr.ifr_addr;
	ip = (char *)inet_ntoa(sin->sin_addr);
	local_ip = ip;
}

void initConfigWatch(ngx_alpaca_client_main_conf_t *aclc, ngx_http_request_t *r){
	zh = zookeeper_init((char *)aclc->zookeeper_addr.data, watcher, 10000, 0, 0, 0);
	if(!zh){
		ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				"zookeeper init fail! the address is \"%V\" ",
				aclc->zookeeper_addr);
		return;
	}
	//struct Stat stat;
	int rc;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	int i = 0;
	setDefault();
	char buffer[ZOOKEEPERBUFSIZE];
	for(i = 0; i< zookeeper_key_length; i++){
		int buflen = ZOOKEEPERBUFSIZE;
		memset(buffer, 0, buflen);
		char *keyname = ngx_pcalloc(r->pool, sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key[i]) + 1);
		if(!keyname){
			continue;
		}
		sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key[i]);
		rc = zoo_get(zh, keyname, 1, buffer, &buflen, NULL);
		if(rc != 0){
			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			  "get key from zookeeper fail! the zookeeper address is \"%V\" ",
			  aclc->zookeeper_addr);//may be should use ngx_str_t
			//fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
		}else{
			if(buflen < ZOOKEEPERBUFSIZE){
				rc = parsebuf(buffer, zookeeper_key[i]);//TODO, add a argv
			}
			if(rc != 0){
				/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				  "get key from zookeeper but parse fail! the zookeeper address is \"%V\" ",
				  aclc->zookeeper_addr);//may be should use ngx_str_t
				//	fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
			}
		}
	}
	aclc->zh = 1;
}

void setDefault(){
	switchconfig.enable = 0;
	switchconfig.pushBlockEvent = 0;
	switchconfig.mount = 0;
	switchconfig.blockByVid = 0;
	switchconfig.clientHeartbeatEnable = 0;
	switchconfig.blockByVidOnly = 0;
	responsemessageconfig.denyMessage = malloc(sizeof(DEFAULTDENYMESSAGE));
	if(responsemessageconfig.denyMessage){
		strcpy(responsemessageconfig.denyMessage, DEFAULTDENYMESSAGE);
	}
	responsemessageconfig.denyRateMessage = malloc(sizeof(DEFAULTDENYRATE));
	if(responsemessageconfig.denyRateMessage){
		strcpy(responsemessageconfig.denyRateMessage, DEFAULTDENYRATE);
	}
	commonconfig.clientDisableUrl = malloc(sizeof(DEFAULT_CLIENT_URL_DISABLE));
	if(commonconfig.clientDisableUrl){
		strcpy(commonconfig.clientDisableUrl, DEFAULT_CLIENT_URL_DISABLE);
	}
	commonconfig.clientEnableUrl = malloc(sizeof(DEFAULT_CLIENT_URL_ENABLE));
	if(commonconfig.clientEnableUrl){
		strcpy(commonconfig.clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	}
	commonconfig.clientHeartbeatInterval = malloc(sizeof(int));
	if(commonconfig.clientHeartbeatInterval){
		*commonconfig.clientHeartbeatInterval = DEFAULT_CLIENT_HEARTBEAT_INTERVAL;
	}
	commonconfig.clientStatusUrl = malloc(sizeof(DEFAULT_CLIENT_URL_STATUS));
	if(commonconfig.clientStatusUrl){
		strcpy(commonconfig.clientStatusUrl, DEFAULT_CLIENT_URL_STATUS);
	}
	commonconfig.clientValidateCodeUrl = malloc(sizeof(DEFAULT_CLIENT_URL_VALIDATECODE));
	if(commonconfig.clientValidateCodeUrl){
		strcpy(commonconfig.clientValidateCodeUrl, DEFAULT_CLIENT_URL_VALIDATECODE);
	}
	commonconfig.serverRoot = malloc(sizeof(DEFAULT_SERVERROOT));
	if(commonconfig.serverRoot){
		strcpy(commonconfig.serverRoot, DEFAULT_SERVERROOT);
	}
	commonconfig.serverBlockEventUrl = malloc(sizeof(DEFAULT_SERVER_URL_BLOCK_EVENT));
	if(commonconfig.serverBlockEventUrl){
		strcpy(commonconfig.serverBlockEventUrl, DEFAULT_SERVER_URL_BLOCK_EVENT);
	}
	commonconfig.serverHeartbeatUrl = malloc(sizeof(DEFAULT_SERVER_URL_HEARTBEAT));
	if(commonconfig.serverHeartbeatUrl){
		strcpy(commonconfig.serverHeartbeatUrl, DEFAULT_SERVER_URL_HEARTBEAT);
	}
}

char* getCharPInstance(char* buf){
	int len = strlen(buf);
	char* result = (char*)malloc(len + 1);
	if(result == NULL){
		return NULL;
	}
	memset(result, 0, len + 1);
	memcpy(result, buf, len);
	return result;
}

int setCharP(char* buf, char** key){
	char *tmp = getCharPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{
		if(*key){
			char *before = *key;
			*key = tmp;
			free(before);
		}
		else{
			*key = tmp;
		}
		return 0;
	}
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

int setIntPDigit(char* buf, int** key){
	int* tmp = getIntPInstanceDigit(buf);
	if(!tmp){
		return -1;
	}
	else{
		if(*key){
			int* before = *key;
			*key = tmp;
			free(before);
		}
		else{
			*key = tmp;
		}
		return 0;
	}
}

int getIntInstance(char* buf){
	if(strcmp(buf, "true") == 0){
		return 1;
	}
	else{
		return 0;
	}
}

int setInt(char* buf, int* key){
	*key = getIntInstance(buf);
	return 0;
}

List* getListPInstance(char* buf){
	cJSON *json, *tmp_json;
	json=cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		List* result = malloc(sizeof(List));
		if(!result){
			cJSON_Delete(json);
			return NULL;
		}
		char** list = (char**)malloc(sizeof(char*) * itemsize);
		if(!list){
			cJSON_Delete(json);
			free(result);	
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			list[i] = cJSON_Print(tmp_json);
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}

}

int setListP(char* buf, List** key){
	List* tmp = getListPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{	
		if(*key){
			List* before = *key;
			*key = tmp;
			int i;
			for(i = 0; i < before->len; i++){
				free(before->list[i]);
			}
			free(before->list);
			free(before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

PairList* getPairListPInstance(char* buf){
	cJSON *json, *tmp_json;
	json = cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		PairList* result = malloc(sizeof(PairList));
		if(!result){
			cJSON_Delete(json);
			return NULL;
		}
		Pair* list = (Pair*)malloc(sizeof(Pair)*itemsize);
		if(!list){
			cJSON_Delete(json);
			free(result);
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			list[i].key = cJSON_Print_key(tmp_json);
			list[i].value = cJSON_Print(tmp_json);
		}
		result->list = list;
		result->len = itemsize;
		return result;
	}
}

int setPairListP(char* buf, PairList** key){
	PairList* tmp = getPairListPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{	
		if(*key){
			PairList* before = *key;
			*key = tmp;
			int i;
			for(i = 0; i < before->len; i++){
				free(before->list[i].key);
				free(before->list[i].value);
			}
			free(before->list);
			free(before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

TripleList* getTripleListPInstance(char* buf){
	cJSON *json, *tmp_json;
	char* pair;
	json = cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		TripleList* result = malloc(sizeof(TripleList));
		if(!result){	
			cJSON_Delete(json);	
			return NULL;
		}
		Triple* list = (Triple*)malloc(sizeof(Triple)*itemsize);
		if(list == NULL){
			cJSON_Delete(json);	
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			pair = cJSON_Print_key(tmp_json);
			char* pch = strtok(pair, ",\"");
			list[i].key.key = pch;
			pch = strtok(NULL, ",\"");
			list[i].key.value = pch;
			list[i].value = cJSON_Print(tmp_json);
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}
}

int setTripleListP(char* buf, TripleList** key){
	TripleList* tmp = getTripleListPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{	
		if(*key){
			TripleList* before = *key;
			*key = tmp;
			int i;
			for(i = 0; i < before->len; i++){
				free(before->list[i].key.key);
				free(before->list[i].key.value);// may be don`t need
				free(before->list[i].value);
			}
			free(before->list);
			free(before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

ListList* getListListPInstance(char *buf){
	cJSON *json, *tmp_json, *sub_tmp_json;
	json = cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		ListList* result = malloc(sizeof(ListList));
		if(!result){	
			cJSON_Delete(json);	
			return NULL;
		}
		List* list = (List*)malloc(sizeof(List)*itemsize);
		if(list == NULL){
			cJSON_Delete(json);
			free(result);	
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json = cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			int subitemsize = cJSON_GetArraySize(tmp_json);
			int j;
			list[i].list = malloc(sizeof(char*) * subitemsize);
			for(j = 0; j < subitemsize; j++){
				sub_tmp_json = cJSON_GetArrayItem(tmp_json,j);
				list[i].list[j] = cJSON_Print(sub_tmp_json);
			}
			list[i].len = subitemsize;
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}
}

int setListListP(char* buf, ListList** key){
	ListList* tmp = getListListPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{	
		if(*key){
			ListList* before = *key;
			*key = tmp;
			int i, j;
			for(i = 0; i < before->len; i++){
				for(j = 0; j < before->list[i].len; j++){
					free(before->list[i].list[j]);
				}
				free(before->list[i].list);
			}
			free(before->list);
			free(before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

int parsebuf(char *buf, char *key){
	if(strcmp(key, "alpaca.filter.enable") == 0){
		return setInt(buf, &switchconfig.enable);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddress") == 0){
		return setListP(buf, &policyconfig.denyIPAddress);
	}
	else if(strcmp(key,"alpaca.filter.pushBlockEvent") == 0){
		return setInt(buf, &switchconfig.pushBlockEvent);
	}
	else if(strcmp(key, "alpaca.filter.mount") == 0){
		return setInt(buf, &switchconfig.mount);
	}
	else if(strcmp(key, "alpaca.client.clientHeartbeatEnable") == 0){
		return setInt(buf, &switchconfig.clientHeartbeatEnable);
	}
	else if(strcmp(key, "alpaca.filter.blockByVid") == 0){
		return setInt(buf, &switchconfig.blockByVid);
	}
	else if(strcmp(key, "alpaca.policy.acceptIPPrefix") == 0){
		return setListP(buf, &policyconfig.acceptIPAddressPrefix);
	}
	else if(strcmp(key, "alpaca.policy.acceptHttpMethod") == 0){
		return setListP(buf, &policyconfig.acceptHttpMethod);
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgent") == 0){
		return setListP(buf, &policyconfig.denyUserAgent);
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgentPrefix") == 0){
		return setListP(buf, &policyconfig.denyUserAgentPrefix);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddressPrefix") == 0){
		return setListP(buf, &policyconfig.denyIPAddressPrefix);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddressRate") == 0){
		return setPairListP(buf, &(policyconfig.denyIPAddressRate));
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgentContainAnd") == 0){
		return setListListP(buf, &policyconfig.denyUserAgentContainAnd);
	}
	else if(strcmp(key, "alpaca.policy.denyIPVidRate") == 0){
		return setTripleListP(buf, &policyconfig.denyIPVidRate);
	}
	else if(strcmp(key, "alpaca.policy.denyNoVisitorIdURL.new") == 0){
		return setPairListP(buf, &policyconfig.denyNOVisitorIDURL);
	}
	else if(strcmp(key, "alpaca.message.denyrate") == 0){
		return setCharP(buf,&responsemessageconfig.denyRateMessage);
	}
	else if(strcmp(key, "alpaca.url.clientStatusUrl") == 0){
		return setCharP(buf,&commonconfig.clientStatusUrl);
	}
	else if(strcmp(key, "alpaca.url.clientEnableUrl") == 0){
		return setCharP(buf,&commonconfig.clientEnableUrl);
	}
	else if(strcmp(key, "alpaca.url.clientDisableUrl") == 0){
		return setCharP(buf,&commonconfig.clientDisableUrl);
	}
	else if(strcmp(key, "alpaca.url.clientValidateCodeUrl") == 0){
		return setCharP(buf,&commonconfig.clientValidateCodeUrl);
	}
	else if(strcmp(key, "alpaca.client.heartbeat.interval") == 0){
		return setIntPDigit(buf,&commonconfig.clientHeartbeatInterval);
	}
	else if(strcmp(key, "alpaca.url.serverRootUrl") == 0){
		return setCharP(buf,&commonconfig.serverRoot);
	}
	else if(strcmp(key, "alpaca.url.serverBlockEventNotifyUrl") == 0){
		return setCharP(buf, &commonconfig.serverBlockEventUrl);
	}
	else if(strcmp(key, "alpaca.url.serverHeartbeatUrl") == 0){
		return setCharP(buf, &commonconfig.serverHeartbeatUrl);
	}
	else if(strcmp(key, "alpaca.filter.blockByVidOnly") == 0){
		return setInt(buf, &switchconfig.blockByVidOnly);
	}
	else if(strcmp(key, "alpaca.policy.denyVisterID") == 0){
		return setListP(buf, &policyconfig.denyVistorID);
	}
	else if(strcmp(key, "alpaca.policy.denyVisterIDRate") == 0){
		return setPairListP(buf, &policyconfig.denyVistorIDRate);
	}
	else{
		return -1;
	}
}


void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx) {
	struct Stat stat;
	int rc;
	int i;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	char buffer[ZOOKEEPERBUFSIZE];
	int buflen = ZOOKEEPERBUFSIZE;
	for(i = 0; i < zookeeper_key_length; i++){
		memset(buffer, 0, buflen);
		char keyname[sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key[i]) + 1];
		sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key[i]);
		if(strcmp(path,keyname) == 0){
			rc = zoo_get(zh, keyname, 1, buffer, &buflen, &stat);
			if(rc == 0){
				if(buflen < ZOOKEEPERBUFSIZE){
					rc = parsebuf(buffer, zookeeper_key[i]);//TODO, buflen
				}
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
	/*struct Stat stat;
	  char buffer[512];
	  int buflen= sizeof(buffer);
	  int rc = zoo_get(zh, "/hupeng", 1, buffer, &buflen, &stat);
	  printf("get data is %s\n",&buffer);*/
}

httpParams_pool* multi_malloc_blockEvent(Context* context){
	int paramnum = PUSH_BLOCK_ARGS_NUM;
	alpaca_memory_pool* pool = alpaca_memory_pool_create(POOL_SIZE);
	Pair* httpParams = alpaca_memory_poll_malloc(pool, sizeof(Pair)*paramnum);
	if(!httpParams){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[0].key = alpaca_memory_poll_malloc(pool, strlen("blockUrl") + 1);
	if(!httpParams[0].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[0].value = alpaca_memory_poll_malloc(pool, context->rawUrl_len + 1);
	if(!httpParams[0].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[1].key = alpaca_memory_poll_malloc(pool,strlen("status") + 1);
	if(!httpParams[1].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[1].value = alpaca_memory_poll_malloc(pool, 4);
	if(!httpParams[1].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[2].key = alpaca_memory_poll_malloc(pool, strlen("blockIp") + 1);
	if(!httpParams[2].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	if(!context->clientIP){
		httpParams[2].value = alpaca_memory_poll_malloc(pool, strlen("empty ip") + 1);
		if(!httpParams[2].value){
			alpaca_memory_poll_destroy(pool);
			return NULL;
		}
	}
	else{
		httpParams[2].value = alpaca_memory_poll_malloc(pool, context->clientIP_len + 1);
		if(!httpParams[2].value){
			alpaca_memory_poll_destroy(pool);
			return NULL;
		}
	}
	httpParams[3].key = alpaca_memory_poll_malloc(pool, strlen("userAgent") + 1);
	if(!httpParams[3].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[3].value = alpaca_memory_poll_malloc(pool, context->userAgent_len + 1);
	if(!httpParams[3].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[4].key = alpaca_memory_poll_malloc(pool, strlen("httpMethod"));
	if(!httpParams[4].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[4].value = alpaca_memory_poll_malloc(pool, context->httpMethod_len + 1);
	if(!httpParams[4].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[5].key = alpaca_memory_poll_malloc(pool, strlen("clientIP") + 1);
	if(!httpParams[5].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[5].value = alpaca_memory_poll_malloc(pool, strlen(local_ip) + 1);
	if(!httpParams[5].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[6].key = alpaca_memory_poll_malloc(pool, strlen("vid") + 1);
	if(!httpParams[6].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	if(!context->visitId){
		httpParams[6].value = alpaca_memory_poll_malloc(pool, strlen("empty ip") + 1);
		if(!httpParams[6].value){
			alpaca_memory_poll_destroy(pool);
			return NULL;
		}
	}
	else{
		httpParams[6].value = alpaca_memory_poll_malloc(pool, context->visitId_len + 1);
		if(!httpParams[6].value){
			alpaca_memory_poll_destroy(pool);
			return NULL;
		}
	}
	httpParams[7].key = alpaca_memory_poll_malloc(pool, strlen("logTime") + 1);
	if(!httpParams[7].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[7].value = alpaca_memory_poll_malloc(pool, strlen("yyyy-MM-dd HH:mm:ss") + 1);
	if(!httpParams[7].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[8].key = alpaca_memory_poll_malloc(pool, strlen(TOKEN_KEY) + 1);
	if(!httpParams[8].key){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams[8].value = alpaca_memory_poll_malloc(pool, MD5_LEN*sizeof(char));
	if(!httpParams[8].value){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	httpParams_pool* p = alpaca_memory_poll_malloc(pool, sizeof(httpParams_pool));
	if(!p){
		alpaca_memory_poll_destroy(pool);
		return NULL;
	}
	p->httpParams = httpParams;
	p->pool = pool;
	return p;
}

int doFilter(ngx_http_request_t *r, ngx_chain_t **out){
	Context* context = NULL;
	if(switchconfig.mount == 1){
		context = getRequestContext(r);
		if(context == NULL){
			return CONTEXTSTATUSNEEDNOTRESPONSE;
		}
		procrequest(r, context);
		if(responseIfNeeded(r, context, out) == CONTEXTSTATUSNEEDRESPONSE){
			if(switchconfig.pushBlockEvent == 1){
				httpParams_pool* p = multi_malloc_blockEvent(context);
				if(!p){
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
				// log when malloc fail
				// log to separate log file
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
	while(num > 0){
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
	handleBlockRequestIfNeeded(context);	
}

int handleInternalRequestIfNeeded(ngx_http_request_t *r, Context *context){
	if(isFirewallRequest(r)){
		if(strncmp((char*)context->rawUrl, commonconfig.clientStatusUrl, context->rawUrl_len) == 0){
			context->status = SHOWSTATUS;
			return 0;
		}
		else if(strncmp((char*)context->rawUrl, commonconfig.clientEnableUrl, context->rawUrl_len) == 0){
			switchconfig.enable = 1;  
			context->status = SHOWSTATUS;
			return 0;
		}
		else if(strncmp((char*)context->rawUrl, commonconfig.clientDisableUrl, context->rawUrl_len) == 0){
			switchconfig.enable = 0;
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

int getCookie(u_char** in, ngx_http_request_t *r){
	if(r->headers_in.cookies.nelts == 0){
		*in = NULL;
		return 0;
	}
	ngx_table_elt_t** cookies = r->headers_in.cookies.elts;
	int i = 0;
	for(i = 0; i < (int)r->headers_in.cookies.nelts; i++){

		*in = (u_char*)strstr((char*)(cookies[i])->value.data, visitId);// _hc.v need config
		if(*in == NULL){
			continue;
		}
		*in = *in + strlen(visitId);
		while(1){
			if(strncmp((char*)*in, "=", 1) == 0 || strncmp((char*)*in, "\"", 1) == 0 || strncmp((char*)*in, " ", 1) == 0 || strncmp((char*)*in, "\\", 1) == 0 ){
				(*in)++;
			}
			else{
				break;
			}
		}
		u_char* end1 = (u_char*)strstr((char*)*in, "\\");
		u_char* end2 = (u_char*)strstr((char*)*in, "\"");
		u_char* end3 = (u_char*)strstr((char*)*in, ";");
		u_char* end = NULL;
		end = (u_char*)((((!end1)?999999999:(U_CHAR)end1) > ((!end2)?999999999:(U_CHAR)end2)) ? ((((!end2)?999999999:(U_CHAR)end2) > ((!end3)?999999999:(U_CHAR)end3))?((!end3)?999999999:(U_CHAR)end3):((!end2)?999999999:(U_CHAR)end2)) : ((((!end1)?999999999:(U_CHAR)end1)>((!end3)?999999999:(U_CHAR)end3))?((!end3)?999999999:(U_CHAR)end3):((!end1)?999999999:(U_CHAR)end1)));

		if(end == NULL){
			*in = NULL;
			return 0;
		}
		return (end - (*in));
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

Context* getRequestContext(ngx_http_request_t *r){
	Context* result = ngx_pcalloc(r->pool, sizeof(Context));
	if(result == NULL){
		return NULL;
	}
	result->userAgent = r->headers_in.user_agent->value.data;
	result->userAgent_len = r->headers_in.user_agent->value.len;
	result->httpMethod = r->method_name.data;
	result->httpMethod_len = r->method_name.len;
	result->clientIP = r->connection->addr_text.data;
	result->clientIP_len = r->connection->addr_text.len;
	u_char* url = r->unparsed_uri.data;
	int url_len = r->unparsed_uri.len;
	result->rawUrl = url;
	result->rawUrl_len = url_len;
	result->visitId_len = getCookie(&result->visitId, r);
	return result;
}

void handleBlockRequestIfNeeded(Context *context){
	if(switchconfig.enable == 1){
		if(startWithIgnoreCaseContains((char*)context->clientIP, policyconfig.acceptIPAddressPrefix) == 1){
			context->status = PASS;
		}
		else if(ignoreCaseContains((char*)context->httpMethod, policyconfig.acceptHttpMethod, context->httpMethod_len) == 0){
			context->status = DENY_HTTPMETHOD;
		}
		else if(context->userAgent == NULL || ignoreCaseContains((char*)context->userAgent, policyconfig.denyUserAgent, context->userAgent_len) || startWithIgnoreCaseContains((char*)context->userAgent, policyconfig.denyUserAgentPrefix)||ignoreCaseContainAll((char*)context->userAgent, policyconfig.denyUserAgentContainAnd)){
			context->status = DENY_USERAGENT;
		}
		else if(context->clientIP == NULL || contains((char*)context->clientIP, policyconfig.denyIPAddress, context->clientIP_len) || startWithIgnoreCaseContains((char*)context->clientIP, policyconfig.denyIPAddressPrefix)){
			context->status = DENY_IP;
		}
		else if(context->visitId != NULL && contains((char*)context->visitId, policyconfig.denyVistorID, context->visitId_len)){
			context->status = DENY_VID;
		}
		else{
			if(context->visitId == NULL){
				if(context->rawUrl != NULL && policyconfig.denyNOVisitorIDURL != NULL){
					int i;
					for(i = 0; i < policyconfig.denyNOVisitorIDURL->len; i++){
						int rawUrl_len = strlen(policyconfig.denyNOVisitorIDURL->list[i].key) - 2;
						if(strncasecmp((char*)context->rawUrl, policyconfig.denyNOVisitorIDURL->list[i].key+1, rawUrl_len) == 0 && (strncasecmp((char*)policyconfig.denyNOVisitorIDURL->list[i].value+1,"all",3) == 0 || strncasecmp((char*)context->httpMethod, policyconfig.denyNOVisitorIDURL->list[i].value+1, context->httpMethod_len) == 0)){
							context->status = DENY_NOVID;
							return;
						}
					}
				}	
				if(policyconfig.denyIPAddressRate != NULL){
					int i;
					for(i = 0; i < policyconfig.denyIPAddressRate->len; i++){
						if(strncmp(policyconfig.denyIPAddressRate->list[i].key + 1, (char*)context->clientIP, context->clientIP_len) == 0){
							if(compareDate(policyconfig.denyIPAddressRate->list[i].value) == 1){
								context->status = DENY_IPRATE;
								return;
							}
						}
					}
				}
			}
			else{
				if(switchconfig.blockByVid == 1){
					if(policyconfig.denyIPVidRate != NULL){
						int i;
						for(i = 0; i < policyconfig.denyIPVidRate->len; i++){
							if(strncmp(policyconfig.denyIPVidRate->list[i].key.key, (char*)context->clientIP, strlen(policyconfig.denyIPVidRate->list[i].key.key)) == 0 && strncmp(policyconfig.denyIPVidRate->list[i].key.value, (char*)context->visitId, strlen(policyconfig.denyIPVidRate->list[i].key.value)) == 0){

								if(compareDate(policyconfig.denyIPVidRate->list[i].value) == 1){
									context->status = DENY_IPVIDRATE;
									return;
								}
							}
						}	
					}
				}
				else{
					if(policyconfig.denyIPAddressRate != NULL){
						int i;
						for(i = 0; i < policyconfig.denyIPAddressRate->len; i++){
							if(strncmp(policyconfig.denyIPAddressRate->list[i].key+1, (char*)context->clientIP, strlen(policyconfig.denyIPAddressRate->list[i].key)-2) == 0){

								if(compareDate(policyconfig.denyIPAddressRate->list[i].value) == 1){
									context->status = DENY_IPRATE;
									return;
								}
							}	
						}
					}
				}
				if(switchconfig.blockByVidOnly == 1){
					if(policyconfig.denyVistorIDRate != NULL){
						int i;
						for(i = 0; i < policyconfig.denyVistorIDRate->len; i++){
							if(strncmp(policyconfig.denyVistorIDRate->list[i].key+1, (char*)context->visitId, strlen(policyconfig.denyVistorIDRate->list[i].key)-2) == 0){

								if(compareDate(policyconfig.denyVistorIDRate->list[i].value) == 1){
									context->status = DENY_VIDRATE;
									return;
								}
							}	
						}
					}
				}

			}
		}
	}

}

int compareDate(char* forbidDate){
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
	cJSON* alpacastatus = dumpStatus();
	ngx_buf_t    *b;  
	b = ngx_calloc_buf(r->pool);  
	if (b == NULL) {
		cJSON_Delete(alpacastatus);	
		return;  
	} 
	char *resbody = cJSON_Print(alpacastatus);
	if(resbody == NULL){
		cJSON_Delete(alpacastatus);//TODO
		return;
	}
	char *res = ngx_pcalloc(r->pool, strlen(resbody) + 1);
	strcpy(res, resbody);
	free(resbody);
	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_length_n = strlen(res);
	b->pos = (u_char *) res;
	b->last = b->pos + strlen(res);  
	b->memory = 1;  
	b->last_buf = 1;  
	(*out) = ngx_alloc_chain_link(r->pool);  
	if (*out == NULL){
		cJSON_Delete(alpacastatus);//TODO
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

cJSON* dumpStatus(){
	cJSON* obj;
	cJSON* item;
	obj = cJSON_CreateObject();
	item = cJSON_CreateBool(switchconfig.enable);
	cJSON_AddItemToObject(obj, zookeeper_key[0], item);
	/*if(switchconfig.running != NULL){
	  item = cJSON_CreateBool(*switchconfig.running);
	  cJSON_AddItemToObject(obj, "running", item);
	  }*/
	item = cJSON_CreateBool(switchconfig.pushBlockEvent);
	cJSON_AddItemToObject(obj, zookeeper_key[2], item);
	item = cJSON_CreateBool(switchconfig.mount);
	cJSON_AddItemToObject(obj, zookeeper_key[3], item);
	item = cJSON_CreateBool(switchconfig.blockByVid);
	cJSON_AddItemToObject(obj, zookeeper_key[5], item);
	if(policyconfig.acceptIPAddressPrefix != NULL){
		item = formatCharPP(policyconfig.acceptIPAddressPrefix->list, policyconfig.acceptIPAddressPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[6], item);
	}
	if(policyconfig.acceptHttpMethod != NULL){
		item = formatCharPP(policyconfig.acceptHttpMethod->list, policyconfig.acceptHttpMethod->len);
		cJSON_AddItemToObject(obj, zookeeper_key[7], item);
	}
	if(policyconfig.denyUserAgent != NULL){
		item = formatCharPP(policyconfig.denyUserAgent->list, policyconfig.denyUserAgent->len);
		cJSON_AddItemToObject(obj, zookeeper_key[8], item);
	}
	if(policyconfig.denyUserAgentPrefix != NULL){
		item = formatCharPP(policyconfig.denyUserAgentPrefix->list, policyconfig.denyUserAgentPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[9], item);
	}
	if(policyconfig.denyIPAddress != NULL){
		item = formatCharPP(policyconfig.denyIPAddress->list, policyconfig.denyIPAddress->len);
		cJSON_AddItemToObject(obj, zookeeper_key[1], item);
	}
	if(policyconfig.denyIPAddressPrefix != NULL){
		item = formatCharPP(policyconfig.denyIPAddressPrefix->list, policyconfig.denyIPAddressPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[10], item);
	}
	if(policyconfig.denyIPAddressRate != NULL){
		item = formatPairPP(policyconfig.denyIPAddressRate->list, policyconfig.denyIPAddressRate->len);
		cJSON_AddItemToObject(obj, zookeeper_key[11], item);
	}
	if(policyconfig.denyUserAgentContainAnd != NULL){
		item = formatListPP(policyconfig.denyUserAgentContainAnd->list, policyconfig.denyUserAgentContainAnd->len);
		cJSON_AddItemToObject(obj, zookeeper_key[12], item);
	}
	if(policyconfig.denyIPVidRateStr != NULL){
		item = formatPairPP(policyconfig.denyIPVidRateStr->list, policyconfig.denyIPVidRateStr->len);
		cJSON_AddItemToObject(obj, zookeeper_key[13], item);
	}
	if(policyconfig.denyNOVisitorIDURL != NULL){
		item = formatPairPP(policyconfig.denyNOVisitorIDURL->list, policyconfig.denyNOVisitorIDURL->len);
		cJSON_AddItemToObject(obj, zookeeper_key[14], item);
	}
	if(responsemessageconfig.denyMessage != NULL){
		item = cJSON_CreateString(responsemessageconfig.denyMessage);
		cJSON_AddItemToObject(obj, "alpaca.message.deny", item);
	}
	if(responsemessageconfig.denyRateMessage != NULL){
		item = cJSON_CreateString(responsemessageconfig.denyRateMessage);
		cJSON_AddItemToObject(obj, zookeeper_key[20], item);
	}
	return obj;
}

cJSON* formatCharPP(char** key, int key_len){
	int i;
	cJSON *obj;
	obj = cJSON_CreateArray();
	cJSON *item;
	for(i = 0; i < key_len; i++){
		item = cJSON_CreateString(key[i]);
		cJSON_AddItemToArray(obj, item);
	}
	return obj;
}

cJSON* formatPairPP(Pair* key, int key_len){
	int i;
	cJSON *obj;
	obj = cJSON_CreateObject();
	cJSON *item;
	for(i = 0; i< key_len; i++){
		item = cJSON_CreateString(key[i].value);
		cJSON_AddItemToObject(obj, key[i].key, item);
	}
	return obj;
}

cJSON* formatListPP(List* key, int key_len){
	int i, j;
	cJSON *obj;
	obj = cJSON_CreateArray();
	cJSON *item;
	cJSON *list = NULL;
	for(i = 0; i < key_len; i++){
		for(j = 0; j < key[i].len; j++){
			list = cJSON_CreateArray();
			item = cJSON_CreateString(key[i].list[j]);
			cJSON_AddItemToArray(list, item);
		}
		cJSON_AddItemToArray(obj,list);
	}
	return obj;
}

char* getResponseDenyMessage(ngx_http_request_t *r, Context *context){
	char* result = ngx_pcalloc(r->pool, DENYMESSAGEMAXLENTH);
	if(result == NULL){
		return NULL;
	}
	int denymessage_len = strlen(responsemessageconfig.denyMessage);
	int i, j, k;
	k = 0;
	for(i = 0; i < denymessage_len; i++){
		if(responsemessageconfig.denyMessage[i] == '$' && responsemessageconfig.denyMessage[i + 1] == '{'){
			j = i + 2;
			int num = 0;
			while(j < denymessage_len && responsemessageconfig.denyMessage[j] != '}'){
				num = num * 10 + atoi(&responsemessageconfig.denyMessage[j]);
				j++;
			}
			if(num == 1){
				char buf[3];
				int compute = (int)context->status;
				sprintf(buf, "%d%d%d", compute/100, (compute/10)%10, compute%100);
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
			result[k] = responsemessageconfig.denyMessage[i];
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
	int denymessage_len = strlen(responsemessageconfig.denyRateMessage);
	int i, j, k;
	k = 0;
	for(i = 0; i < denymessage_len; i++){
		if(responsemessageconfig.denyRateMessage[i] == '$' && responsemessageconfig.denyRateMessage[i + 1] == '{'){
			j = i + 2;
			int num = 0;
			while(j < denymessage_len && responsemessageconfig.denyMessage[j] != '}'){
				num = num * 10 + atoi(&responsemessageconfig.denyMessage[j]);
				j++;
			}
			if(num == 1){
				int len = strlen((char*)context->clientIP);
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
			result[k] = responsemessageconfig.denyRateMessage[i];
			k++;
		}
	}
	return result;
}
