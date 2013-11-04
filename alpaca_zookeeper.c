#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "alpacaClient.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "switchconfig.h"
#include "policyconfig.h"
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
#define DEFAULT_DUMP_INFO_SIZE 1024*1024

void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
int parsebuf(char *buf, char *key);
void set_default();
void set_string(char* buf, char* volatile* key);
void set_digit(char* buf, int volatile* key);
void set_int(char* buf, int volatile* key);
//cJSON* formatCharPP(char** key, int key_len);
//cJSON* formatPairPP(Pair* key, int key_len);
//cJSON* formatListPP(List* key, int key_len);

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

void alpaca_strcat_colon(char** dst, char* key, char* value){
	strcat(*dst, "\"");
	strcat(*dst, key);
	strcat(*dst, "\"");
	strcat(*dst, ":");
	strcat(*dst, "\"");
	strcat(*dst, value);
	strcat(*dst, "\"");
}

void alpaca_strcat_no_colon(char** dst, char* key, char* value){
	strcat(*dst, "\"");
	strcat(*dst, key);
	strcat(*dst, "\"");
	strcat(*dst, ":");
	strcat(*dst, value);
}

char* dumpStatus(){
	char* alpaca_status = malloc(DEFAULT_DUMP_INFO_SIZE);
	if(!alpaca_status){
		return NULL; 
	}
	ngx_memset(alpaca_status, 0, DEFAULT_DUMP_INFO_SIZE);
	strcat(alpaca_status, "{");

	alpaca_strcat_no_colon(&alpaca_status, "alpaca.filter.enable", switchconfig->string_enable);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.filter.pushBlockEvent", switchconfig->string_pushBlockEvent);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.filter.mount", switchconfig->string_mount);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.client.blockByVid", switchconfig->string_blockByVid);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.acceptIPPrefix", policyconfig->acceptIPAddressPrefix);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.acceptHttpMethod", policyconfig->acceptHttpMethod);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyUserAgent", policyconfig->denyUserAgent);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyUserAgentPrefix", policyconfig->denyUserAgentPrefix);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyIPAddress", policyconfig->denyIPAddress);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyIPAddressPrefix", policyconfig->denyIPAddressPrefix);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyIPAddressRate", policyconfig->denyIPAddressRate);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyIPVidRate", policyconfig->denyIPVidRate);
	strcat(alpaca_status, ",");
	alpaca_strcat_no_colon(&alpaca_status, "alpaca.policy.withdomain.denyNoVisitorIdURL.new", policyconfig->denyNOVisitorIDURL);
//	strcat(alpaca_status, ",");
//	alpaca_strcat_colon(&alpaca_status, "alpaca.message.deny", responsemessageconfig->denyMessage);
//	strcat(alpaca_status, ",");
//	alpaca_strcat_colon(&alpaca_status, "alpaca.message.denyrate", responsemessageconfig->denyRateMessage);
	strcat(alpaca_status, "}");
	return alpaca_status;
}
