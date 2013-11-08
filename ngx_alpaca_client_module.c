

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_channel.h>

#include <zookeeper/zookeeper.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <curl/curl.h>
#include <pthread.h>

#include "alpacaClient.h"
#include "responsemessageconfig.h"
#include "commonconfig.h"
#include "switchconfig.h"
#include "alpaca_log.h"
#include "alpaca_zookeeper.h"
#include "alpaca_heartbeat.h"
#include "alpaca_get_local_ip.h"
#include "policyconfig.h"
#include "alpaca_constant.h"


int config_denymessage = 0;
int config_denyratemessage = 0;
u_char* denymessage;
u_char* denyratemessage;
char* visitId;
int allow_ua_empty = 0;
u_char* zookeeper_addr;
lua_State* L;
char* lua_filename;
int send_process_listen_port = 0;
int volatile denyIPAddressRateExpire;
int volatile denyIPVidRateExpire;
int volatile denyVisterIDRateExpire;
int volatile acceptIPPrefixCount = 0;
static char pipe_buf[DEFAULT_ALPACA_PIPE_BUF];
static int pipe_buf_start;
static int pipe_buf_end;

typedef struct {
	ngx_flag_t       enable;
	ngx_uint_t       port;
	ngx_socket_t     fd;
} ngx_proc_send_conf_t;


ngx_int_t ngx_alpaca_init_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_alpaca_client_handler(ngx_http_request_t *r);
static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void *ngx_alpaca_client_create_main_conf(ngx_conf_t *cf);
char *ngx_alpaca_client_init_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
void getVisitId(ngx_alpaca_client_main_conf_t *aclc);

static void *ngx_proc_send_create_conf(ngx_conf_t *cf);
static char *ngx_proc_send_merge_conf(ngx_conf_t *cf, void *parent,
		void *child);
static ngx_int_t ngx_proc_send_prepare(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_send_process_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_send_loop(ngx_cycle_t *cycle);
static void ngx_proc_send_exit_process(ngx_cycle_t *cycle);
static void ngx_proc_send_accept(ngx_event_t *ev);

alpaca_pipe_t alpaca_pipe[ALPACA_MAX_PROCESS];

static ngx_command_t ngx_proc_send_commands[] = {

	{ ngx_string("listen"),
		NGX_PROC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_PROC_CONF_OFFSET,
		offsetof(ngx_proc_send_conf_t, port),
		NULL },

	{ ngx_string("daytime"),
		NGX_PROC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_PROC_CONF_OFFSET,
		offsetof(ngx_proc_send_conf_t, enable),
		NULL },

	ngx_null_command
};


static ngx_proc_module_t ngx_proc_send_module_ctx = {
	ngx_string("send"),
	NULL,
	NULL,
	ngx_proc_send_create_conf,
	ngx_proc_send_merge_conf,
	ngx_proc_send_prepare,
	ngx_proc_send_process_init,
	ngx_proc_send_loop,
	ngx_proc_send_exit_process
};


ngx_module_t ngx_proc_send_module = {
	NGX_MODULE_V1,
	&ngx_proc_send_module_ctx,
	ngx_proc_send_commands,
	NGX_PROC_MODULE,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NGX_MODULE_V1_PADDING
};

static ngx_command_t  ngx_alpaca_client_commands[] = {

	{ ngx_string("alpaca"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, enable),
		NULL },
	{ ngx_string("interface"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, interface),
		NULL },
	{ ngx_string("alpaca_zk"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, zookeeper_addr),
		NULL },
	{ ngx_string("alpaca_vid"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, visitId),
		NULL },
	{ ngx_string("alpaca_log"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, log),
		NULL },
	{ ngx_string("alpaca_log_level"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, level),
		NULL },
	{ ngx_string("alpaca_allow_ua_empty"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, allow_ua_empty),
		NULL },
	{ ngx_string("alpaca_denymessage"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, denymessage),
		NULL },
	{ ngx_string("alpaca_denyratemessage"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, denyratemessage),
		NULL },
	{ ngx_string("lua_file"),
		NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, lua_file),
		NULL },

	ngx_null_command
};

static ngx_http_module_t  ngx_alpaca_client_module_ctx = {
	NULL,                                  /* preconfiguration */
	ngx_alpaca_client_init,              /* postconfiguration */

	ngx_alpaca_client_create_main_conf,                                  /* create main configuration */
	NULL,                                  /* init main configuration */

	NULL,                                  /* create server configuration */
	NULL,                                  /* merge server configuration */

	NULL,       /* create location configuration */
	NULL        /* merge location configuration */
};


ngx_module_t  ngx_alpaca_client_module = {
	NGX_MODULE_V1,
	&ngx_alpaca_client_module_ctx,       /* module context */
	ngx_alpaca_client_commands,          /* module directives */
	NGX_HTTP_MODULE,                       /* module type */
	NULL,                                  /* init master */
	NULL,                                  /* init module */
	&ngx_alpaca_init_process,                                  /* init process */
	NULL,                                  /* init thread */
	NULL,                                  /* exit thread */
	NULL,                                  /* exit process */
	NULL,                                  /* exit master */
	NGX_MODULE_V1_PADDING
};

void set_table(char* buf, char* value, ngx_log_t* log){
	lua_getglobal(L,"decode");              
	lua_pushstring(L, value);
	lua_pushstring(L, buf);

	int ret = lua_pcall(L,2,0,0);
	if(ret){
		ngx_log_error(NGX_LOG_ERR, log, ngx_errno, "%s update fail!", value);
	}
}

void update_zk_value(char* key, char* buf, ngx_event_t *ev){
	//ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "%s, %s", key, buf);
	if(ngx_strcmp(key, "alpaca.filter.enable") == 0){
		set_int(buf, &switchconfig->enable);
		set_string(buf, &switchconfig->string_enable);
	}
	else if(ngx_strcmp(key,"alpaca.filter.pushBlockEvent") == 0){
		set_int(buf, &switchconfig->pushBlockEvent);
		set_string(buf, &switchconfig->string_pushBlockEvent);
	}
	else if(ngx_strcmp(key, "alpaca.filter.mount") == 0){
		set_int(buf, &switchconfig->mount);
		set_string(buf, &switchconfig->string_mount);
	}
	else if(ngx_strcmp(key, "alpaca.client.clientHeartbeatEnable") == 0){
		set_int(buf, &switchconfig->clientHeartbeatEnable);
		set_string(buf, &switchconfig->string_clientHeartbeatEnable);
	}
	else if(ngx_strcmp(key, "alpaca.filter.blockByVid") == 0){
		set_int(buf, &switchconfig->blockByVid);
		set_string(buf, &switchconfig->string_blockByVid);
		set_table(buf, "alpaca.filter.blockByVid", ev->log);
	}
	else if(ngx_strcmp(key, "alpaca.filter.blockByVidOnly") == 0){
		set_int(buf, &switchconfig->blockByVidOnly);
		set_string(buf, &switchconfig->string_blockByVidOnly);
		set_table(buf, "alpaca.filter.blockByVidOnly", ev->log);
	}
	else if(ngx_strcmp(key, "alpaca.client.heartbeat.interval") == 0){
		set_digit(buf, &commonconfig->clientHeartbeatInterval);
		set_string(buf, &commonconfig->string_clientHeartbeatInterval);
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
	else if(ngx_strcmp(key, "alpaca.message.denyrate") == 0 && !config_denyratemessage){
		set_string(buf, &responsemessageconfig->denyRateMessage);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyIPAddress") == 0){
		set_table(buf, "alpaca.policy.denyIPAddress", ev->log);
		set_string(buf, &policyconfig->denyIPAddress);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.acceptIPPrefix") == 0){
		acceptIPPrefixCount++;
		set_table(buf, "alpaca.policy.acceptIPPrefix", ev->log);
		set_string(buf, &policyconfig->acceptIPAddressPrefix);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.acceptHttpMethod") == 0){
		set_table(buf, "alpaca.policy.acceptHttpMethod", ev->log);
		set_string(buf, &policyconfig->acceptHttpMethod);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyUserAgent") == 0){
		set_table(buf, "alpaca.policy.denyUserAgent", ev->log);
		set_string(buf, &policyconfig->denyUserAgent);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyUserAgentPrefix") == 0){
		set_table(buf, "alpaca.policy.denyUserAgentPrefix", ev->log);
		set_string(buf, &policyconfig->denyUserAgentPrefix);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyIPAddressPrefix") == 0){
		set_table(buf, "alpaca.policy.denyIPAddressPrefix", ev->log);
		set_string(buf, &policyconfig->denyIPAddressPrefix);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyIPAddressRate") == 0){
		denyIPAddressRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
		set_table(buf, "alpaca.policy.denyIPAddressRate", ev->log);
		set_string(buf, &policyconfig->denyIPAddressRate);
	}
	else if(ngx_strcmp(key, "alpaca.policy.denyUserAgentContainAnd") == 0){
		set_table(buf, "alpaca.policy.denyUserAgentContainAnd", ev->log);
		set_string(buf, &policyconfig->denyUserAgentContainAnd);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyIPVidRate") == 0){
		denyIPVidRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
		set_table(buf, "alpaca.policy.denyIPVidRate", ev->log);
		set_string(buf, &policyconfig->denyIPVidRate);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyNoVisitorIdURL.new") == 0){
		set_table(buf, "alpaca.policy.denyNoVisitorIdURL.new", ev->log);
		set_string(buf, &policyconfig->denyNOVisitorIDURL);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyVisterID") == 0){
		set_table(buf, "alpaca.policy.denyVisterID", ev->log);
		set_string(buf, &policyconfig->denyVistorID);
	}
	else if(ngx_strcmp(key, "alpaca.policy.withdomain.denyVisterIDRate") == 0){
		denyVisterIDRateExpire = (int)time(NULL) + DEFAULT_LIST_EXPIRE_TIME;
		set_table(buf, "alpaca.policy.denyVisterIDRate", ev->log);
		set_string(buf, &policyconfig->denyVistorIDRate);
	}
	else{
	}

}

static void ngx_pipe_handler(ngx_event_t *ev){
	char tmp[DEFAULT_PIPE_SIZE];
	char keyname[DEFAULT_ALPACA_KEY_MAX_LEN];
	char value[DEFAULT_ALPACA_PIPE_BUF];
	ngx_memset(tmp, 0, DEFAULT_PIPE_SIZE);
	ngx_memset(keyname, 0, DEFAULT_ALPACA_KEY_MAX_LEN);
	ngx_memset(value, 0, DEFAULT_ALPACA_PIPE_BUF);
	int num = 0;
	while(1){
		num = read(alpaca_pipe[ngx_process_slot].pipefd[1], tmp, DEFAULT_PIPE_SIZE);
		if(num < 0){
			ngx_memset(tmp, 0, DEFAULT_PIPE_SIZE);
			alpaca_log_wirte(ALPACA_WARN, " pipe empty!");
			break;
		}
		if(num == 0){
			continue;
		}
		if(pipe_buf_start <= pipe_buf_end){
			if(num <= DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end){
				ngx_memcpy(pipe_buf + pipe_buf_end, tmp, num);
				pipe_buf_end = pipe_buf_end + num;
			}
	//		else{
	//			if(num < DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end + pipe_buf_start){
	//				ngx_memcpy(pipe_buf + pipe_buf_end, tmp, DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end);
	//				ngx_memcpy(pipe_buf, tmp + DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end, num - (DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end));
	//				pipe_buf_end = num - (DEFAULT_ALPACA_PIPE_BUF - pipe_buf_end);
	//			}
	//			else{
	//			}
	//		}
		}
//		else{
//			if(num < pipe_buf_start - pipe_buf_end){
//				ngx_memcpy(pipe_buf + pipe_buf_end, tmp, num);
//				pipe_buf_end = pipe_buf_end + num;
//			}
//		}
		ngx_memset(tmp, 0, DEFAULT_PIPE_SIZE);
		//ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "recieve  pipe buffer %d", num);
	}

	ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "recieve from pipe  %d, %d, %s", pipe_buf_start, pipe_buf_end, pipe_buf);
	char* start = NULL;
	char* end = NULL;
	char* point = pipe_buf + pipe_buf_start;
	while(1){
		if(pipe_buf_end == pipe_buf_start){
			pipe_buf_end = 0;
			pipe_buf_start = 0;
			ngx_memset(pipe_buf, 0, DEFAULT_ALPACA_PIPE_BUF);
			//ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "pipe done!");
			break;
		}

		start = point;
		end = strstr(start, "\r\n");
		if(!end || (unsigned long)end - (unsigned long)pipe_buf > (unsigned long)pipe_buf_end){
			break;
		}
		if(((unsigned long)end - (unsigned long)start) < ngx_strlen(ZOOKEEPERROUTE)){
			point = point + 2;
			continue;
		}
		ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "update keyname length %d", (end - start));
		ngx_memcpy(keyname, start + ngx_strlen(ZOOKEEPERROUTE), (end - start) - ngx_strlen(ZOOKEEPERROUTE));
		start = end + 2;
		end = NULL;
		end = strstr(start, "\r\r\n\n");
		if(!end || (unsigned long)end  - (unsigned long)pipe_buf > (unsigned long)pipe_buf_end){
			//ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "can`t resolve %s", start);
			ngx_memset(keyname, 0, DEFAULT_ALPACA_KEY_MAX_LEN);
			ngx_memset(value, 0, DEFAULT_ALPACA_PIPE_BUF);
			break;
		}
		//ngx_log_error(NGX_LOG_INFO, ev->log, ngx_errno, "update value length %d", (end - start));
		ngx_memcpy(value, start, (end - start));
		pipe_buf_start = (unsigned long)end - (unsigned long)point + 4 + pipe_buf_start;
		update_zk_value(keyname, value, ev);
		ngx_memset(keyname, 0, DEFAULT_ALPACA_KEY_MAX_LEN);
		ngx_memset(value, 0, DEFAULT_ALPACA_PIPE_BUF);
		point = end + 4;
	}
}

ngx_int_t    
ngx_alpaca_init_process(ngx_cycle_t *cycle){
	if (ngx_add_channel_event(cycle, alpaca_pipe[ngx_process_slot].pipefd[1], NGX_READ_EVENT,
				ngx_pipe_handler)
			== NGX_ERROR)
	{
		/* fatal */
		exit(2);
	}
	init();
	L = luaL_newstate();
	luaL_openlibs(L);
	if (luaL_loadfile(L,lua_filename) || lua_pcall(L,0,0,0)) {
		return NGX_ERROR;
	}

	switchconfig = malloc(sizeof(SwitchConfig));
	commonconfig = malloc(sizeof(CommonConfig));
	policyconfig = malloc(sizeof(PolicyConfig));
	responsemessageconfig = malloc(sizeof(ResponseMessageConfig));
	ngx_memset(switchconfig, 0, sizeof(SwitchConfig));
	ngx_memset(commonconfig, 0, sizeof(CommonConfig));
	ngx_memset(policyconfig, 0, sizeof(PolicyConfig));
	ngx_memset(responsemessageconfig, 0, sizeof(ResponseMessageConfig));
	set_default();

	set_table("{\"post\":[\"all\"],\"get\":[\"all\"]}", "alpaca.policy.acceptHttpMethod", cycle->log);
	set_string("{\"post\":[\"all\"],\"get\":[\"all\"]}", &policyconfig->acceptHttpMethod);

	CURL *curl;
	curl = curl_easy_init();
	char url[100];
	ngx_memset(url, 0, 100);
	sprintf(url, "%s:%d/", "http://127.0.0.1", send_process_listen_port);	
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
	curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return NGX_OK;
}

	static ngx_int_t
ngx_alpaca_client_handler(ngx_http_request_t *r)
{
	ngx_alpaca_client_main_conf_t *ahlf;
	ngx_int_t                  rc;
	ngx_chain_t                *out = NULL;

	ahlf = ngx_http_get_module_main_conf(r, ngx_alpaca_client_module);

	if(ahlf->enable == 1){ 
		if(doFilter(r, &out) == CONTEXTSTATUSNEEDNOTRESPONSE){
			return NGX_DECLINED;
		}
		rc = ngx_http_send_header(r);
		if(rc == NGX_ERROR){
			return rc;
		}
		rc = ngx_http_output_filter(r, out);
		if(rc != NGX_DECLINED){
			ngx_http_finalize_request(r, rc);
			return NGX_DONE;
		}
		return rc;
	}else{
		return NGX_DECLINED;
	}
}

	static ngx_int_t
ngx_alpaca_client_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt        *h;
	ngx_http_core_main_conf_t  *cmcf;
	ngx_alpaca_client_main_conf_t *conf;
	ngx_core_conf_t   *ccf;

	conf = ngx_http_conf_get_module_main_conf(cf, ngx_alpaca_client_module);
	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	zookeeper_addr = conf->zookeeper_addr.data;
	getLocalIP((char *)conf->interface.data, local_ip);
	getVisitId(conf);
	allow_ua_empty = conf->allow_ua_empty;
	if(conf->denymessage.len != 0){
		denymessage = conf->denymessage.data;
		config_denymessage = 1;
	}
	if(conf->denyratemessage.len != 0){
		denyratemessage = conf->denyratemessage.data;
		config_denyratemessage = 1;
	}
	lua_filename = (char*)conf->lua_file.data;

	alpaca_log_open((char*)conf->log.data, (char*)conf->level.data);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_alpaca_client_handler;

	ccf = (ngx_core_conf_t *) ngx_get_conf(cf->cycle->conf_ctx, ngx_core_module);
	int i = 0;
	for(i = 0; i < ccf->worker_processes; i++){
		socketpair(AF_UNIX, SOCK_STREAM, 0, alpaca_pipe[i].pipefd);
		int flags;
		flags = fcntl(alpaca_pipe[i].pipefd[1], F_GETFL);
		flags |= O_NONBLOCK;
		if (fcntl(alpaca_pipe[i].pipefd[1], F_SETFL, flags) == -1) {
			perror("fcntl");
			exit(1);
		}
	}

	alpaca_worker_processes = ccf->worker_processes;
	return NGX_OK;
}

	static void *
ngx_alpaca_client_create_main_conf(ngx_conf_t *cf)
{
	ngx_alpaca_client_main_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_alpaca_client_main_conf_t));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}
	conf->zookeeper_addr.len = 0;
	conf->zookeeper_addr.data = NULL;
	conf->visitId.len = 0;
	conf->visitId.data = NULL;
	conf->enable = NGX_CONF_UNSET;
	conf->interface.data = NULL;
	conf->interface.len = 0;
	conf->log.len = 0;
	conf->log.data = NULL;
	conf->level.len = 0;
	conf->level.data = NULL;
	conf->allow_ua_empty = 0;
	conf->denymessage.len = 0;
	conf->denymessage.data = NULL;
	conf->denyratemessage.len = 0;
	conf->denyratemessage.data = NULL;
	return conf;
}

void getVisitId(ngx_alpaca_client_main_conf_t *aclc){
	if(aclc->visitId.data == NULL){
		visitId = malloc(strlen(DEFAULT_VISIT_ID) + 1);
		if(visitId == NULL){
			alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get visitid");
			return;
		}
		memset(visitId, 0, aclc->visitId.len + 1);
		strcpy(visitId, DEFAULT_VISIT_ID);
	}
	else{
		visitId = malloc(aclc->visitId.len + 1);
		if(visitId == NULL){
			alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get visitid");
			return;
		}
		memset(visitId, 0, aclc->visitId.len + 1);
		strcpy(visitId, (char*)aclc->visitId.data);
	}
}

	static void *
ngx_proc_send_create_conf(ngx_conf_t *cf)
{
	ngx_proc_send_conf_t  *pbcf;

	pbcf = ngx_pcalloc(cf->pool, sizeof(ngx_proc_send_conf_t));

	if (pbcf == NULL) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
				"send create proc conf error");
		return NULL;
	}

	pbcf->enable = NGX_CONF_UNSET;
	pbcf->port = NGX_CONF_UNSET_UINT;

	return pbcf;
}


	static char *
ngx_proc_send_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_proc_send_conf_t  *prev = parent;
	ngx_proc_send_conf_t  *conf = child;

	ngx_conf_merge_uint_value(conf->port, prev->port, 0);
	ngx_conf_merge_off_value(conf->enable, prev->enable, 0);

	return NGX_CONF_OK;
}


	static ngx_int_t
ngx_proc_send_prepare(ngx_cycle_t *cycle)
{
	ngx_proc_send_conf_t  *pbcf;

	pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_send_module);
	if (!pbcf->enable) {
		return NGX_DECLINED;
	}

	if (pbcf->port == 0) {
		return NGX_DECLINED;
	}

	send_process_listen_port = pbcf->port;

	return NGX_OK;
}

	static ngx_int_t
ngx_proc_send_process_init(ngx_cycle_t *cycle)
{
	int                       reuseaddr;
	ngx_event_t              *rev;
	ngx_socket_t              fd;
	ngx_connection_t         *c;
	struct sockaddr_in        sin;
	ngx_proc_send_conf_t  *pbcf;

	pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_send_module);
	fd = ngx_socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "send socket error");
		return NGX_ERROR;
	}

	reuseaddr = 1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
				(const void *) &reuseaddr, sizeof(int))
			== -1)
	{
		ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
				"send setsockopt(SO_REUSEADDR) failed");

		ngx_close_socket(fd);
		return NGX_ERROR;
	}
	if (ngx_nonblocking(fd) == -1) {
		ngx_log_error(NGX_LOG_EMERG, cycle->log, ngx_socket_errno,
				"send nonblocking failed");

		ngx_close_socket(fd);
		return NGX_ERROR;
	}

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(pbcf->port);

	if (bind(fd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "send bind error");
		return NGX_ERROR;
	}

	if (listen(fd, 20) == -1) {
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "send listen error");
		return NGX_ERROR;
	}

	c = ngx_get_connection(fd, cycle->log);
	if (c == NULL) {
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "send no connection");
		return NGX_ERROR;
	}

	c->log = cycle->log;
	rev = c->read;
	rev->log = c->log;
	rev->accept = 1;
	rev->handler = ngx_proc_send_accept;

	if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {
		return NGX_ERROR;
	}

	pbcf->fd = fd;

	switchconfig = malloc(sizeof(SwitchConfig));
	commonconfig = malloc(sizeof(CommonConfig));
	ngx_memset(switchconfig, 0, sizeof(SwitchConfig));
	ngx_memset(commonconfig, 0, sizeof(CommonConfig));

	init_config_watch(zookeeper_addr);

	return NGX_OK;
}


	static ngx_int_t
ngx_proc_send_loop(ngx_cycle_t *cycle)
{
	pthread_t tid;
	void* (*ptr)(void *arg);
	ptr = heartbeatcycle;
	int err1 = pthread_create(&tid, NULL, ptr, cycle);
	if(err1){
		ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "create heartbeatcycle thread fail!" );
	}
	sleep(1);
	return NGX_OK;
}


	static void
ngx_proc_send_exit_process(ngx_cycle_t *cycle)
{
	ngx_proc_send_conf_t *pbcf;

	pbcf = ngx_proc_get_conf(cycle->conf_ctx, ngx_proc_send_module);

	ngx_close_socket(pbcf->fd);
}


	static void
ngx_proc_send_accept(ngx_event_t *ev)
{
	u_char             sa[NGX_SOCKADDRLEN];
	socklen_t          socklen;
	ngx_socket_t       s;
	ngx_connection_t  *lc;
	char command[4096];
	char keyname[200];
	ngx_memset(command, 0, 4096);
	ngx_memset(keyname, 0, 200);

	lc = ev->data;
	s = accept(lc->fd, (struct sockaddr *) sa, &socklen);
	if (s == -1) {
		return;
	}

	if (ngx_nonblocking(s) == -1) {
		goto finish;
	}
	int len = read(s, command, 4096);
	ngx_log_error(NGX_LOG_INFO, ev->log, 0, "pipe command %d, %s", len, command);
	char* start;
	char* end;
	start = strstr(command, "/");
	if(!start){
		return;
	}
	end = strstr(start, " ");
	if(!end){
		return;
	}
	ngx_memcpy(keyname, start, (long)end - (long)start);
	if(ngx_memcmp(keyname, "/", 1) == 0){
		register_zk_value();
	}
	else if(ngx_memcmp(keyname, "/denyIPAddressRate", ngx_strlen("/denyIPAddressRate")) == 0){
		get_zk_value(keyname, -1);
	}
	else if(ngx_memcmp(keyname, "/denyIPVidRate", ngx_strlen("/denyIPVidRate")) == 0){
		get_zk_value(keyname, -1);
	}
	else if(ngx_memcmp(keyname, "/denyVisterIDRate", ngx_strlen("/denyVisterIDRate")) == 0){
		get_zk_value(keyname, -1);
	}
	else if(ngx_memcmp(keyname, "/acceptIPPrefix", ngx_strlen("/acceptIPPrefix")) == 0){
		get_zk_value(keyname, -1);
	}

finish:
	ngx_close_socket(s);
}
