#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <zookeeper/zookeeper.h>

#include "alpacaClient.h"
#include "policyconfig.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "switchconfig.h"
#include "alpaca_log.h"
#include "alpaca_zookeeper.h"

#define ZOOKEEPERBUFSIZE 20480
#define ZOOKEEPERROUTE "/DP/CONFIG/"
#define DEFAULTDENYMESSAGE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#2583ad;text-decoration:none;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}p{{text-align:center;}}sub{{display:block;margin:0;padding:0;color:#aaa;font-size:11px;text-align:right;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><p>对不起，您的访问存在某些问题。<br />如果您是正常访问，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，并附上以下信息：<br /><textarea rows=\"10\" cols=\"80\">${0}\r\n${1}\r\n${2}</textarea></p><sub>${0}</sub><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_agent\");</script></body></html>"
#define DEFAULT_CLIENT_URL_DISABLE "/dianping.firewall.client.disable"
#define DEFAULT_CLIENT_URL_ENABLE "/dianping.firewall.client.enable"
#define DEFAULT_CLIENT_HEARTBEAT_INTERVAL 180
#define DEFAULT_CLIENT_URL_STATUS "/dianping.firewall.client.status"
#define DEFAULT_CLIENT_URL_VALIDATECODE "/deny.code"
#define DEFAULT_SERVERROOT "http://192.168.26.38:8888"
#define DEFAULT_SERVER_URL_BLOCK_EVENT "/clientManagement/dianping.firewall.server.blockevent"
#define DEFAULT_SERVER_URL_HEARTBEAT "/clientManagement/dianping.firewall.server.heartbeat"
#define DEFAULTDENYRATE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#ccc;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}form{{padding:8px;font-size:14px;line-height:18px;text-align:center;}}form input{{font-size:20px;font-weight:bold;}}form input.i{{width:190px;}}p{{margin-bottom:30px;}}div{{margin-bottom:8px;}}p.c{{color:#ccc;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><form method=\"post\" action=\"/validcode\"><p>对不起，你访问的太快了，请输入验证码后继续浏览：</p><div><img  id=\"code\" src=\"/deny.code\" alt=\"验证码\" /></div><div> <input name=\"vode\" class=\"i\" type=\"text\" /><input type=\"submit\" value=\" 提 交 \" /><input type=\"hidden\" name=\"referer\" value=\"hupeng\" /></div><p class=\"c\">如果您(${0})经常碰到此情况，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，我们会尽快处理。</p></form><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_rate\");</script></body></html>"

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf, char *key);
void setDefault();
char* getCharPInstance(char* buf);
int setCharP(char* buf, char* volatile* key);
int setIntDigit(char* buf, int volatile* key);
int getIntInstance(char* buf);
int setInt(char* buf, int volatile* key);
List* getListPInstance(char* buf);
int setListP(char* buf, List* volatile* key);
PairList* getPairListPInstance(char* buf);
int setPairListP(char* buf, PairList* volatile* key);
TripleList* getTripleListPInstance(char* buf);
int setTripleListP(char* buf, TripleList* volatile* key);
ListList* getListListPInstance(char *buf);
int setListListP(char* buf, ListList* volatile* key);
cJSON* formatCharPP(char** key, int key_len);
cJSON* formatPairPP(Pair* key, int key_len);
cJSON* formatListPP(List* key, int key_len);

ngx_slab_pool_t* shpool;
static zhandle_t *zh;
static char* zookeeper_key[] = {"alpaca.filter.enable", "alpaca.policy.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.acceptIPPrefix", "alpaca.policy.acceptHttpMethod", "alpaca.policy.denyUserAgent", "alpaca.policy.denyUserAgentPrefix", "alpaca.policy.denyIPAddressPrefix", "alpaca.policy.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.denyIPVidRate", "alpaca.policy.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.denyVisterID", "alpaca.policy.denyVisterIDRate"}; 

void get_zk_value(char* keyname, char* buffer, int buflen, int i){
		int rc;
		rc = zoo_get(zh, keyname, 1, buffer, &buflen, NULL);//TODO refactor
		if(rc != 0){
			alpaca_log_wirte(ALPACA_WARN, "zookeeper get fail");
			/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
			  "get key from zookeeper fail! the zookeeper address is \"%V\" ",
			  aclc->zookeeper_addr);//may be should use ngx_str_t
			//fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
		}else{
			rc = parsebuf(buffer, zookeeper_key[i]);//TODO, add a argv
			if(rc != 0){
				alpaca_log_wirte(ALPACA_WARN, "zookeeper value parse fail");
				/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				  "get key from zookeeper but parse fail! the zookeeper address is \"%V\" ",
				  aclc->zookeeper_addr);//may be should use ngx_str_t
				//	fprintf(stderr, "Error %d for %s\n", rc, __LINE__);*/
			}
		}

}
void initConfigWatch(u_char* zookeeper_addr){
	zh = zookeeper_init((char*)zookeeper_addr, watcher, 10000, 0, 0, 0);
	if(!zh){
		alpaca_log_wirte(ALPACA_ERROR, "init zookeeper fail");
		/*ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
		  "zookeeper init fail! the address is \"%V\" ",
		  aclc->zookeeper_addr);
		  */
		return;
	}
	//struct Stat stat;
	int zookeeper_key_length = sizeof(zookeeper_key)/sizeof(char*);
	int i = 0;
	setDefault();
	char buffer[ZOOKEEPERBUFSIZE];//TODO check size
	for(i = 0; i< zookeeper_key_length; i++){
		int buflen = ZOOKEEPERBUFSIZE;
		memset(buffer, 0, buflen);
		char keyname[sizeof(ZOOKEEPERROUTE) + strlen(zookeeper_key[i]) + 1];
		sprintf(keyname, "%s%s", ZOOKEEPERROUTE, zookeeper_key[i]);
		get_zk_value(keyname, buffer, buflen, i);
	}
}

void setDefault(){
	switchconfig->enable = 0;
	switchconfig->pushBlockEvent = 0;
	switchconfig->mount = 0;
	switchconfig->blockByVid = 0;
	switchconfig->clientHeartbeatEnable = 0;
	switchconfig->blockByVidOnly = 0;
	/*set_default_string(responsemessageconfig.denyMessage, DEFAULTDENYMESSAGE);
	set_default_string(responsemessageconfig.denyRateMessage, DEFAULTDENYRATE);
	set_default_string(commonconfig.clientDisableUrl, DEFAULT_CLIENT_URL_DISABLE);
	set_default_string(commonconfig.clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	set_default_string(commonconfig.clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	commonconfig.clientHeartbeatInterval = DEFAULT_CLIENT_HEARTBEAT_INTERVAL;
	set_default_string(commonconfig.clientStatusUrl, DEFAULT_CLIENT_URL_STATUS);
	set_default_string(commonconfig.clientValidateCodeUrl, DEFAULT_CLIENT_URL_VALIDATECODE);
	set_default_string(commonconfig.serverRoot, DEFAULT_SERVERROOT);
	set_default_string(commonconfig.serverBlockEventUrl, DEFAULT_SERVER_URL_BLOCK_EVENT);
	set_default_string(commonconfig.serverHeartbeatUrl, DEFAULT_SERVER_URL_HEARTBEAT);*/
	responsemessageconfig->denyMessage = ngx_slab_alloc(shpool, sizeof(DEFAULTDENYMESSAGE));
	if(responsemessageconfig->denyMessage){
		strcpy(responsemessageconfig->denyMessage, DEFAULTDENYMESSAGE);
	}
	responsemessageconfig->denyRateMessage = ngx_slab_alloc(shpool, sizeof(DEFAULTDENYRATE));
	if(responsemessageconfig->denyRateMessage){
		strcpy(responsemessageconfig->denyRateMessage, DEFAULTDENYRATE);
	}
	commonconfig->clientDisableUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_CLIENT_URL_DISABLE));
	if(commonconfig->clientDisableUrl){
		strcpy(commonconfig->clientDisableUrl, DEFAULT_CLIENT_URL_DISABLE);
	}
	commonconfig->clientEnableUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_CLIENT_URL_ENABLE));
	if(commonconfig->clientEnableUrl){
		strcpy(commonconfig->clientEnableUrl, DEFAULT_CLIENT_URL_ENABLE);
	}
	commonconfig->clientHeartbeatInterval = DEFAULT_CLIENT_HEARTBEAT_INTERVAL;
	commonconfig->clientStatusUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_CLIENT_URL_STATUS));
	if(commonconfig->clientStatusUrl){
		strcpy(commonconfig->clientStatusUrl, DEFAULT_CLIENT_URL_STATUS);
	}
	commonconfig->clientValidateCodeUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_CLIENT_URL_VALIDATECODE));
	if(commonconfig->clientValidateCodeUrl){
		strcpy(commonconfig->clientValidateCodeUrl, DEFAULT_CLIENT_URL_VALIDATECODE);
	}
	commonconfig->serverRoot = ngx_slab_alloc(shpool, sizeof(DEFAULT_SERVERROOT));
	if(commonconfig->serverRoot){
		strcpy(commonconfig->serverRoot, DEFAULT_SERVERROOT);
	}
	commonconfig->serverBlockEventUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_SERVER_URL_BLOCK_EVENT));
	if(commonconfig->serverBlockEventUrl){
		strcpy(commonconfig->serverBlockEventUrl, DEFAULT_SERVER_URL_BLOCK_EVENT);
	}
	commonconfig->serverHeartbeatUrl = ngx_slab_alloc(shpool, sizeof(DEFAULT_SERVER_URL_HEARTBEAT));
	if(commonconfig->serverHeartbeatUrl){
		strcpy(commonconfig->serverHeartbeatUrl, DEFAULT_SERVER_URL_HEARTBEAT);
	}
}

char* getCharPInstance(char* buf){
	char* result = (char*)ngx_slab_alloc(shpool, strlen(buf) + 1);
	if(result == NULL){
		return NULL;
	}
	memset(result, 0, strlen(buf) + 1);
	strcpy(result, buf);
	return result;
}

int setCharP(char* buf, char* volatile* key){//TODO P to Ptr
	char *tmp = getCharPInstance(buf);
	if(!tmp){
		return -1;
	}
	else{
		if(*key){
			char *before = *key;
			*key = tmp;
			ngx_slab_free(shpool, before);
		}
		else{
			*key = tmp;
		}
		return 0;
	}
}


int setIntDigit(char* buf, int volatile* key){
	if(!buf){
		return -1;
	}
	int tmp = atoi(buf);
	if(tmp > 0){
		*key = tmp;
		return 0;
	}
	return -1;
}

int getIntInstance(char* buf){
	if(strcmp(buf, "true") == 0){
		return 1;
	}
	else{
		return 0;
	}
}

int setInt(char* buf, int volatile* key){
	*key = getIntInstance(buf);
	return 0;
}

List* getListPInstance(char* buf){
	cJSON *json, *tmp_json;
	char* tmp;
	json=cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		List* result = ngx_slab_alloc(shpool, sizeof(List));
		if(!result){
			cJSON_Delete(json);
			return NULL;
		}
		char** list = (char**)ngx_slab_alloc(shpool, sizeof(char*) * itemsize);
		if(!list){
			cJSON_Delete(json);
			ngx_slab_free(shpool, result);
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				list[i] = NULL;//TODO need add?
				continue;
			}
			tmp = cJSON_Print(tmp_json);
			if(!tmp){
				list[i] = NULL;
				continue;
			}
			else{
				list[i] = (char*)ngx_slab_alloc(shpool, strlen(tmp) + 1);
				if(!list[i]){
					continue;
				}
				memset(list[i], 0, strlen(tmp) + 1);
				strcpy(list[i], tmp);
				free(tmp);
			}
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}

}

int setListP(char* buf, List* volatile *key){
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
				ngx_slab_free(shpool, before->list[i]);
			}
			ngx_slab_free(shpool, before->list);
			ngx_slab_free(shpool, before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

PairList* getPairListPInstance(char* buf){
	cJSON *json, *tmp_json;
	char* tmp1;
	char* tmp2;
	json = cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		PairList* result = ngx_slab_alloc(shpool, sizeof(PairList));
		if(!result){
			cJSON_Delete(json);
			return NULL;
		}
		memset(result, 0, sizeof(PairList));
		Pair* list = (Pair*)ngx_slab_alloc(shpool, sizeof(Pair)*itemsize);
		if(!list){
			cJSON_Delete(json);
			ngx_slab_free(shpool, result);
			return NULL;
		}
		memset(list, 0, sizeof(Pair)*itemsize);
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			tmp1 = cJSON_Print_key(tmp_json);
			if(!tmp1){
				list[i].key = NULL;
				continue;
			}
			else{
				list[i].key = ngx_slab_alloc(shpool, strlen(tmp1) + 1);
				if(!list[i].key){
					continue;
				}
				memset(list[i].key, 0, strlen(tmp1) + 1);
				strcpy(list[i].key, tmp1);
				free(tmp1);
			}
			tmp2 = cJSON_Print(tmp_json);
			if(!tmp2){
				list[i].value = NULL;
				continue;
			}
			else{
				list[i].value = ngx_slab_alloc(shpool, strlen(tmp2) + 1);
				if(!list[i].value){
					continue;
				}
				memset(list[i].value, 0, strlen(tmp2) + 1);
				strcpy(list[i].value, tmp2);
				free(tmp2);
			}
		}
		result->list = list;
		result->len = itemsize;
		return result;
	}
}

int setPairListP(char* buf, PairList* volatile* key){
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
				ngx_slab_free(shpool, before->list[i].key);
				ngx_slab_free(shpool, before->list[i].value);
			}
			ngx_slab_free(shpool, before->list);
			ngx_slab_free(shpool, before);
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
		TripleList* result = ngx_slab_alloc(shpool, sizeof(TripleList));
		if(!result){	
			cJSON_Delete(json);	
			return NULL;
		}
		Triple* list = (Triple*)ngx_slab_alloc(shpool, sizeof(Triple)*itemsize);
		if(list == NULL){
			cJSON_Delete(json);	
			ngx_slab_free(shpool, result);
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json=cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			pair = cJSON_Print_key(tmp_json);
			if(!pair){
				list[i].key.key = NULL;
				list[i].key.value = NULL;
				continue;
			}
			else{
				char* pch = strtok(pair, ",\"");
				if(!pch){
					list[i].key.key = NULL;
					list[i].key.value = NULL;
					free(pair);
					continue;
				}
				list[i].key.key = ngx_slab_alloc(shpool, strlen(pch) + 1);
				if(!list[i].key.key){
					list[i].key.key = NULL;
					list[i].key.value = NULL;
					free(pair);
					continue;
				}
				strcpy(list[i].key.key, pch);
				pch = strtok(NULL, ",\"");
				if(!pch){
					ngx_slab_free(shpool, list[i].key.key);
					list[i].key.key = NULL;
					list[i].key.value = NULL;
					free(pair);
					continue;
				}
				list[i].key.value = ngx_slab_alloc(shpool, strlen(pch) + 1);
				if(!list[i].key.value){
					ngx_slab_free(shpool, list[i].key.value);
					list[i].key.key = NULL;
					list[i].key.value = NULL;
					free(pair);
					continue;
				}
				strcpy(list[i].key.value, pch);
			}
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}
}

int setTripleListP(char* buf, TripleList* volatile* key){
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
				ngx_slab_free(shpool, before->list[i].key.key);
				ngx_slab_free(shpool, before->list[i].key.value);
				ngx_slab_free(shpool, before->list[i].value);
			}
			ngx_slab_free(shpool, before->list);
			ngx_slab_free(shpool, before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

ListList* getListListPInstance(char *buf){
	cJSON *json, *tmp_json, *sub_tmp_json;
	char* tmp;
	json = cJSON_Parse(buf);
	if (!json) {
		return NULL;
	}
	else
	{
		int itemsize = cJSON_GetArraySize(json);
		int i = 0;
		ListList* result = ngx_slab_alloc(shpool, sizeof(ListList));
		if(!result){	
			cJSON_Delete(json);	
			return NULL;
		}
		List* list = (List*)ngx_slab_alloc(shpool, sizeof(List)*itemsize);
		if(list == NULL){
			cJSON_Delete(json);
			ngx_slab_free(shpool, result);
			return NULL;
		}
		for(i = 0; i < itemsize; i++){
			tmp_json = cJSON_GetArrayItem(json,i);
			if(!tmp_json){
				continue;
			}
			int subitemsize = cJSON_GetArraySize(tmp_json);
			int j;
			list[i].list = ngx_slab_alloc(shpool, sizeof(char*) * subitemsize);
			if(!list[i].list){
				list[i].list = NULL;
				continue;
			}
			for(j = 0; j < subitemsize; j++){
				sub_tmp_json = cJSON_GetArrayItem(tmp_json,j);
				tmp = cJSON_Print(sub_tmp_json);
				list[i].list[j] = ngx_slab_alloc(shpool, strlen(tmp) + 1);
				if(!list[i].list[j]){
					free(tmp);
					continue;
				}
				memset(list[i].list[j], 0 , strlen(tmp) + 1);
				strcpy(list[i].list[j], tmp);
				free(tmp);
			}
			list[i].len = subitemsize;
		}
		result->list = list;
		result->len = itemsize;
		cJSON_Delete(json);
		return result;
	}
}

int setListListP(char* buf, ListList* volatile* key){
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
					ngx_slab_free(shpool, before->list[i].list[j]);
				}
				ngx_slab_free(shpool, before->list[i].list);
			}
			ngx_slab_free(shpool, before->list);
			ngx_slab_free(shpool, before);
		}
		else{
			*key = tmp;
		}
	}
	return 0;
}

int parsebuf(char *buf, char *key){
	if(strcmp(key, "alpaca.filter.enable") == 0){
		return setInt(buf, &switchconfig->enable);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddress") == 0){
		return setListP(buf, &policyconfig->denyIPAddress);//TODO add volatile
	}
	else if(strcmp(key,"alpaca.filter.pushBlockEvent") == 0){
		return setInt(buf, &switchconfig->pushBlockEvent);
	}
	else if(strcmp(key, "alpaca.filter.mount") == 0){
		return setInt(buf, &switchconfig->mount);
	}
	else if(strcmp(key, "alpaca.client.clientHeartbeatEnable") == 0){
		return setInt(buf, &switchconfig->clientHeartbeatEnable);
	}
	else if(strcmp(key, "alpaca.filter.blockByVid") == 0){
		return setInt(buf, &switchconfig->blockByVid);
	}
	else if(strcmp(key, "alpaca.policy.acceptIPPrefix") == 0){
		return setListP(buf, &policyconfig->acceptIPAddressPrefix);
	}
	else if(strcmp(key, "alpaca.policy.acceptHttpMethod") == 0){
		return setListP(buf, &policyconfig->acceptHttpMethod);
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgent") == 0){
		return setListP(buf, &policyconfig->denyUserAgent);
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgentPrefix") == 0){
		return setListP(buf, &policyconfig->denyUserAgentPrefix);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddressPrefix") == 0){
		return setListP(buf, &policyconfig->denyIPAddressPrefix);
	}
	else if(strcmp(key, "alpaca.policy.denyIPAddressRate") == 0){
		return setPairListP(buf, &(policyconfig->denyIPAddressRate));
	}
	else if(strcmp(key, "alpaca.policy.denyUserAgentContainAnd") == 0){
		return setListListP(buf, &policyconfig->denyUserAgentContainAnd);
	}
	else if(strcmp(key, "alpaca.policy.denyIPVidRate") == 0){
		return setTripleListP(buf, &policyconfig->denyIPVidRate);
	}
	else if(strcmp(key, "alpaca.policy.denyNoVisitorIdURL.new") == 0){
		return setPairListP(buf, &policyconfig->denyNOVisitorIDURL);
	}
	else if(strcmp(key, "alpaca.message.denyrate") == 0){
		return setCharP(buf,&responsemessageconfig->denyRateMessage);
	}
	else if(strcmp(key, "alpaca.url.clientStatusUrl") == 0){
		return setCharP(buf,&commonconfig->clientStatusUrl);
	}
	else if(strcmp(key, "alpaca.url.clientEnableUrl") == 0){
		return setCharP(buf,&commonconfig->clientEnableUrl);
	}
	else if(strcmp(key, "alpaca.url.clientDisableUrl") == 0){
		return setCharP(buf,&commonconfig->clientDisableUrl);
	}
	else if(strcmp(key, "alpaca.url.clientValidateCodeUrl") == 0){
		return setCharP(buf,&commonconfig->clientValidateCodeUrl);
	}
	else if(strcmp(key, "alpaca.client.heartbeat.interval") == 0){
		return setIntDigit(buf,&commonconfig->clientHeartbeatInterval);
	}
	else if(strcmp(key, "alpaca.url.serverRootUrl") == 0){
		return setCharP(buf,&commonconfig->serverRoot);
	}
	else if(strcmp(key, "alpaca.url.serverBlockEventNotifyUrl") == 0){
		return setCharP(buf, &commonconfig->serverBlockEventUrl);
	}
	else if(strcmp(key, "alpaca.url.serverHeartbeatUrl") == 0){
		return setCharP(buf, &commonconfig->serverHeartbeatUrl);
	}
	else if(strcmp(key, "alpaca.filter.blockByVidOnly") == 0){
		return setInt(buf, &switchconfig->blockByVidOnly);
	}
	else if(strcmp(key, "alpaca.policy.denyVisterID") == 0){
		return setListP(buf, &policyconfig->denyVistorID);
	}
	else if(strcmp(key, "alpaca.policy.denyVisterIDRate") == 0){
		return setPairListP(buf, &policyconfig->denyVistorIDRate);
	}
	else{
		return -1;
	}
}


void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx) {
	//struct Stat stat;
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
