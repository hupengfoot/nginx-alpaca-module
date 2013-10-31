#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "alpacaClient.h"
#include "policyconfig.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "switchconfig.h"
#include "alpaca_zookeeper.h"
#include "alpaca_log.h"

#define DEFAULTDENYMESSAGE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#2583ad;text-decoration:none;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}p{{text-align:center;}}sub{{display:block;margin:0;padding:0;color:#aaa;font-size:11px;text-align:right;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><p>对不起，您的访问存在某些问题。<br />如果您是正常访问，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，并附上以下信息：<br /><textarea rows=\"10\" cols=\"80\">${0}\r\n${1}\r\n${2}</textarea></p><sub>${0}</sub><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_agent\");</script></body></html>"
#define DEFAULT_CLIENT_URL_DISABLE "/dianping.firewall.client.disable"
#define DEFAULT_CLIENT_URL_ENABLE "/dianping.firewall.client.enable"
#define DEFAULT_CLIENT_HEARTBEAT_INTERVAL 180
#define DEFAULT_CLIENT_URL_STATUS "/dianping.firewall.client.status"
#define DEFAULT_CLIENT_URL_VALIDATECODE "/deny.code"
#define DEFAULT_SERVERROOT "http://192.168.26.48:8080"
#define DEFAULT_SERVER_URL_BLOCK_EVENT "/clientManagement/dianping.firewall.server.blockevent"
#define DEFAULT_SERVER_URL_HEARTBEAT "/clientManagement/dianping.firewall.server.heartbeat"
#define DEFAULTDENYRATE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#ccc;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}form{{padding:8px;font-size:14px;line-height:18px;text-align:center;}}form input{{font-size:20px;font-weight:bold;}}form input.i{{width:190px;}}p{{margin-bottom:30px;}}div{{margin-bottom:8px;}}p.c{{color:#ccc;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><form method=\"post\" action=\"/validcode\"><p>对不起，你访问的太快了，请输入验证码后继续浏览：</p><div><img  id=\"code\" src=\"/deny.code\" alt=\"验证码\" /></div><div> <input name=\"vode\" class=\"i\" type=\"text\" /><input type=\"submit\" value=\" 提 交 \" /><input type=\"hidden\" name=\"referer\" value=\"hupeng\" /></div><p class=\"c\">如果您(${0})经常碰到此情况，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，我们会尽快处理。</p></form><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_rate\");</script></body></html>"

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf, char *key);
void set_default();
void set_string(char* buf, char* volatile* key);
void set_digit(char* buf, int volatile* key);
void set_int(char* buf, int volatile* key);
cJSON* formatCharPP(char** key, int key_len);
cJSON* formatPairPP(Pair* key, int key_len);
cJSON* formatListPP(List* key, int key_len);

extern int config_denymessage;
extern int config_denyratemessage;

extern alpaca_pipe_t alpaca_pipe[ALPACA_MAX_PROCESS];

zhandle_t *zh;
char* zookeeper_key[] = ZOOKEEPERWATCHKEYS;

void get_zk_value(char* keyname, char* buffer, int buflen, int i){
	int rc;
	rc = zoo_get(zh, keyname, 1, buffer, &buflen, NULL);
	if(rc != 0){
		alpaca_log_wirte(ALPACA_WARN, "zookeeper get fail");
	}else{
		parsebuf(buffer, zookeeper_key[i]);//TODO, add a argv
		int i = 0;
		for(i = 0; i < alpaca_worker_processes; i++){
			if(write(alpaca_pipe[i].pipefd[0], keyname, ngx_strlen(keyname)) == -1){
				alpaca_log_wirte(ALPACA_WARN, "write zookeeper info to worker fail!");
			}
			if(write(alpaca_pipe[i].pipefd[0], "\r\n", strlen("\r\n")) == -1){
				alpaca_log_wirte(ALPACA_WARN, "write zookeeper info to worker fail!");
			}
			if(write(alpaca_pipe[i].pipefd[0], buffer, strlen(buffer)) == -1){
				alpaca_log_wirte(ALPACA_WARN, "write zookeeper info to worker fail!");
			}
			if(write(alpaca_pipe[i].pipefd[0], "\r\r\n\n", strlen("\r\r\n\n")) == -1){
				alpaca_log_wirte(ALPACA_WARN, "write zookeeper info to worker fail!");
			}
		}
		if(rc != 0){
			alpaca_log_wirte(ALPACA_WARN, "zookeeper value parse fail");
		}
	}

}

void init_config_watch(u_char* zookeeper_addr){
	zh = zookeeper_init((char*)zookeeper_addr, watcher, 10000, 0, 0, 0);
	if(!zh){
		alpaca_log_wirte(ALPACA_ERROR, "init zookeeper fail");
		return;
	}
	register_zk_value();
}

void register_zk_value(){
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	int i = 0;
	char buffer[ZOOKEEPERBUFSIZE];//TODO check size
	for(i = 0; i < zookeeper_key_length; i++){
		int buflen = ZOOKEEPERBUFSIZE;
		memset(buffer, 0, buflen);
		char keyname[sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key[i]) + 1];
		sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key[i]);
		get_zk_value(keyname, buffer, buflen, i);
	}

}

void set_default(){
	switchconfig->enable = 0;
	switchconfig->pushBlockEvent = 0;
	switchconfig->mount = 0;
	switchconfig->blockByVid = 0;
	switchconfig->clientHeartbeatEnable = 0;
	switchconfig->blockByVidOnly = 0;
	//	set_default_string(commonconfig.clientDisableUrl, DEFAULT_CLIENT_URL_DISABLE);
	//	set_default_string(commonconfig.clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	//	set_default_string(commonconfig.clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	//	commonconfig.clientHeartbeatInterval = DEFAULT_CLIENT_HEARTBEAT_INTERVAL;
	//	set_default_string(commonconfig.clientStatusUrl, DEFAULT_CLIENT_URL_STATUS);
	//	set_default_string(commonconfig.clientValidateCodeUrl, DEFAULT_CLIENT_URL_VALIDATECODE);
	//	set_default_string(commonconfig.serverRoot, DEFAULT_SERVERROOT);
	//	set_default_string(commonconfig.serverBlockEventUrl, DEFAULT_SERVER_URL_BLOCK_EVENT);
	//	set_default_string(commonconfig.serverHeartbeatUrl, DEFAULT_SERVER_URL_HEARTBEAT);
	if(!config_denymessage){
		responsemessageconfig->denyMessage = malloc(sizeof(DEFAULTDENYMESSAGE));
		if(responsemessageconfig->denyMessage){
			strcpy(responsemessageconfig->denyMessage, DEFAULTDENYMESSAGE);
		}
	}
	if(!config_denyratemessage){
		responsemessageconfig->denyRateMessage = malloc(sizeof(DEFAULTDENYRATE));
		if(responsemessageconfig->denyRateMessage){
			strcpy(responsemessageconfig->denyRateMessage, DEFAULTDENYRATE);
		}
	}
}

void set_string(char* buf, char* volatile* key){
	char* tmp = (char*)malloc(strlen(buf) + 1);
	if(tmp == NULL){
		return;
	}
	memset(tmp, 0, strlen(buf) + 1);
	strcpy(tmp, buf);
	if(!tmp){
		return;
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
		return;
	}
}


void set_digit(char* buf, int volatile* key){
	if(!buf){
		return;
	}
	int tmp = atoi(buf);
	if(tmp > 0){
		*key = tmp;
	}
}

void set_int(char* buf, int volatile* key){
	if(strcmp(buf, "true") == 0){
		*key = 1;
	}
	else{
		*key = 0;
	}
}

int parsebuf(char *buf, char *key){
	if(ngx_strcmp(key, "alpaca.filter.enable") == 0){
		set_int(buf, &switchconfig->enable);
	}
	else if(ngx_strcmp(key,"alpaca.filter.pushBlockEvent") == 0){
		set_int(buf, &switchconfig->pushBlockEvent);
	}
	else if(ngx_strcmp(key, "alpaca.filter.mount") == 0){
		set_int(buf, &switchconfig->mount);
	}
	else if(ngx_strcmp(key, "alpaca.client.clientHeartbeatEnable") == 0){
		set_int(buf, &switchconfig->clientHeartbeatEnable);
	}
	else if(ngx_strcmp(key, "alpaca.filter.blockByVid") == 0){
		set_int(buf, &switchconfig->blockByVid);
	}
	else if(ngx_strcmp(key, "alpaca.filter.blockByVidOnly") == 0){
		set_int(buf, &switchconfig->blockByVidOnly);
	}
	else if(ngx_strcmp(key, "alpaca.client.heartbeat.interval") == 0){
		set_digit(buf, &commonconfig->clientHeartbeatInterval);
	}
	else if(ngx_strcmp(key, "alpaca.url.clientStatusUrl") == 0){
		set_string(buf, &commonconfig->clientStatusUrl);
	}
	else if(ngx_strcmp(key, "alpaca.url.clientEnableUrl") == 0){
		set_string(buf, &commonconfig->clientEnableUrl);
	}
	else if(ngx_strcmp(key, "alpaca.url.clientDisableUrl") == 0){
		set_string(buf, &commonconfig->clientDisableUrl);
	}
	else if(ngx_strcmp(key, "alpaca.url.clientValidateCodeUrl") == 0){
		set_string(buf, &commonconfig->clientValidateCodeUrl);
	}
	else if(ngx_strcmp(key, "alpaca.url.serverRootUrl") == 0){
		set_string(buf, &commonconfig->serverRoot);
	}
	else if(ngx_strcmp(key, "alpaca.url.serverBlockEventNotifyUrl") == 0){
		set_string(buf, &commonconfig->serverBlockEventUrl);
	}
	else if(ngx_strcmp(key, "alpaca.url.serverHeartbeatUrl") == 0){
		set_string(buf, &commonconfig->serverHeartbeatUrl);
	}
	else{
		return -1;
	}
}

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx) {
	int i;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	char buffer[ZOOKEEPERBUFSIZE];
	int buflen = ZOOKEEPERBUFSIZE;
	for(i = 0; i < zookeeper_key_length; i++){
		memset(buffer, 0, buflen);
		char keyname[sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key[i]) + 1];
		sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key[i]);
		if(strcmp(path,keyname) == 0){
			get_zk_value(keyname, buffer, buflen, i);
			break;
		}
	}
}

cJSON* dumpStatus(){
	cJSON* obj;
	cJSON* item;
	obj = cJSON_CreateObject();
	item = cJSON_CreateBool(switchconfig->enable);
	cJSON_AddItemToObject(obj, zookeeper_key[0], item);
	/*if(switchconfig.running != NULL){
	  item = cJSON_CreateBool(*switchconfig.running);
	  cJSON_AddItemToObject(obj, "running", item);
	  }*/
	item = cJSON_CreateBool(switchconfig->pushBlockEvent);
	cJSON_AddItemToObject(obj, zookeeper_key[2], item);
	item = cJSON_CreateBool(switchconfig->mount);
	cJSON_AddItemToObject(obj, zookeeper_key[3], item);
	item = cJSON_CreateBool(switchconfig->blockByVid);
	cJSON_AddItemToObject(obj, zookeeper_key[5], item);
	if(policyconfig->acceptIPAddressPrefix != NULL){
		item = formatCharPP(policyconfig->acceptIPAddressPrefix->list, policyconfig->acceptIPAddressPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[6], item);
	}
	if(policyconfig->acceptHttpMethod != NULL){
		item = formatCharPP(policyconfig->acceptHttpMethod->list, policyconfig->acceptHttpMethod->len);
		cJSON_AddItemToObject(obj, zookeeper_key[7], item);
	}
	if(policyconfig->denyUserAgent != NULL){
		item = formatCharPP(policyconfig->denyUserAgent->list, policyconfig->denyUserAgent->len);
		cJSON_AddItemToObject(obj, zookeeper_key[8], item);
	}
	if(policyconfig->denyUserAgentPrefix != NULL){
		item = formatCharPP(policyconfig->denyUserAgentPrefix->list, policyconfig->denyUserAgentPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[9], item);
	}
	if(policyconfig->denyIPAddress != NULL){
		item = formatCharPP(policyconfig->denyIPAddress->list, policyconfig->denyIPAddress->len);
		cJSON_AddItemToObject(obj, zookeeper_key[1], item);
	}
	if(policyconfig->denyIPAddressPrefix != NULL){
		item = formatCharPP(policyconfig->denyIPAddressPrefix->list, policyconfig->denyIPAddressPrefix->len);
		cJSON_AddItemToObject(obj, zookeeper_key[10], item);
	}
	if(policyconfig->denyIPAddressRate != NULL){
		item = formatPairPP(policyconfig->denyIPAddressRate->list, policyconfig->denyIPAddressRate->len);
		cJSON_AddItemToObject(obj, zookeeper_key[11], item);
	}
	if(policyconfig->denyUserAgentContainAnd != NULL){
		item = formatListPP(policyconfig->denyUserAgentContainAnd->list, policyconfig->denyUserAgentContainAnd->len);
		cJSON_AddItemToObject(obj, zookeeper_key[12], item);
	}
	if(policyconfig->denyIPVidRateStr != NULL){
		item = formatPairPP(policyconfig->denyIPVidRateStr->list, policyconfig->denyIPVidRateStr->len);
		cJSON_AddItemToObject(obj, zookeeper_key[13], item);
	}
	if(policyconfig->denyNOVisitorIDURL != NULL){
		item = formatPairPP(policyconfig->denyNOVisitorIDURL->list, policyconfig->denyNOVisitorIDURL->len);
		cJSON_AddItemToObject(obj, zookeeper_key[14], item);
	}
	if(responsemessageconfig->denyMessage != NULL){
		item = cJSON_CreateString(responsemessageconfig->denyMessage);
		cJSON_AddItemToObject(obj, "alpaca.message.deny", item);
	}
	if(responsemessageconfig->denyRateMessage != NULL){
		item = cJSON_CreateString(responsemessageconfig->denyRateMessage);
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
