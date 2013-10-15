

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
#include "alpaca_heartbeat.h"
#include "alpaca_get_local_ip.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define ZOOKEEPER_SHM_SIZE 200*1024*1024
#define DEFAULT_VISIT_ID  "_hc.v"



extern ngx_slab_pool_t* shpool;
//static int inited;
int config_denymessage = 0;
int config_denyratemessage = 0;
u_char* denymessage;
u_char* denyratemessage;
char* local_ip;
char* visitId;
int allow_ua_empty = 0;
u_char* zookeeper_addr;
volatile unsigned long* push_event_num;
lua_State* L;
char* lua_filename;

typedef struct {
	ngx_flag_t       enable;
	ngx_uint_t       port;
	ngx_socket_t     fd;
	ngx_uint_t       mmap_dat_size;
	ngx_str_t        mmap_idx;
	ngx_str_t 	 mmap_dat;
} ngx_proc_send_conf_t;


ngx_int_t ngx_alpaca_init_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_alpaca_client_handler(ngx_http_request_t *r);
static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void *ngx_alpaca_client_create_main_conf(ngx_conf_t *cf);
char *ngx_alpaca_client_init_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_alpaca_client_init_zone(ngx_shm_zone_t *shm_zone, void *data);
void getVisitId(ngx_alpaca_client_main_conf_t *aclc);

static void *ngx_proc_send_create_conf(ngx_conf_t *cf);
static char *ngx_proc_send_merge_conf(ngx_conf_t *cf, void *parent,
		void *child);
static ngx_int_t ngx_proc_send_prepare(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_send_process_init(ngx_cycle_t *cycle);
static ngx_int_t ngx_proc_send_loop(ngx_cycle_t *cycle);
static void ngx_proc_send_exit_process(ngx_cycle_t *cycle);
static void ngx_proc_send_accept(ngx_event_t *ev);
//static char *ngx_alpaca_client_merge_main_conf(ngx_conf_t *cf, void *parent, void *child);


static char  *week[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
	"Friday", "Saturday" };

static char  *months[] = { "January", "February", "March", "April", "May",
	"June", "July", "August", "Semptember", "October",
	"November", "December" };

ngx_socket_t        pipefd[2];

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

	{ ngx_string("mmap_dat_size"),
		NGX_PROC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_num_slot,
		NGX_PROC_CONF_OFFSET,
		offsetof(ngx_proc_send_conf_t, mmap_dat_size),
		NULL },

	{ ngx_string("mmap_dat"),
		NGX_PROC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_PROC_CONF_OFFSET,
		offsetof(ngx_proc_send_conf_t, mmap_dat),
		NULL },

	{ ngx_string("mmap_idx"),
		NGX_PROC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_PROC_CONF_OFFSET,
		offsetof(ngx_proc_send_conf_t, mmap_idx),
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

static void ngx_pipe_handler(ngx_event_t *ev){

	char buf[1024];
	int w,h;
	read(pipefd[1], buf, 1024);
	lua_getglobal(L,"width");
	lua_getglobal(L,"height");
	w = lua_tointeger(L,-2);
	h = lua_tointeger(L,-1);

	lua_getglobal(L,"decode");               //调用lua中的函数sum
	lua_pushinteger(L,buf) ;
	int ret = lua_pcall(L,1,1,0) ;
	ngx_log_error(NGX_LOG_ERR, ev->log, ngx_errno,
			"hellokitty!  %s, %d, %d", buf, w, h);
}

ngx_int_t    
ngx_alpaca_init_process(ngx_cycle_t *cycle){
	if (ngx_add_channel_event(cycle, pipefd[1], NGX_READ_EVENT,
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
	return NGX_OK;
}

	static ngx_int_t
ngx_alpaca_client_handler(ngx_http_request_t *r)
{
	ngx_alpaca_client_main_conf_t *ahlf;
	ngx_int_t                  rc;
	ngx_chain_t                *out = NULL;

	ahlf = ngx_http_get_module_main_conf(r, ngx_alpaca_client_module);
	//	if(!inited){ 
	//		init(ahlf, r);
	//		inited = 1;
	//	}
	if(ahlf->enable == 1){ //TODO reset
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

//static ngx_int_t ngx_alpaca_client_init_zookeeper_shm(ngx_conf_t *cf){
//	ngx_str_t name;
//	name.data = (u_char*) "zookeeper_shm";
//	name.len = strlen("zookeeper_shm");
//	size_t size;
//	size = ZOOKEEPER_SHM_SIZE;
//	ngx_shm_zone_t            *shm_zone;
//	shm_zone = ngx_shared_memory_add(cf, &name, size,
//			&ngx_alpaca_client_module);
//	if (shm_zone == NULL) {
//		return NGX_ERROR;
//	}
//	shm_zone->init = ngx_alpaca_client_init_zone;
//	shm_zone->data = name.data;//TODO how to set this value
//	return NGX_OK;
//}

void get_denymessage_from_config(){
	if(denymessage){
		responsemessageconfig->denyMessage = ngx_slab_alloc(shpool, strlen((char*)denymessage) + 1);
		if(!responsemessageconfig->denyMessage){
			return;
		}
		memset(responsemessageconfig->denyMessage, 0, strlen((char*)denymessage) + 1);
		strcpy(responsemessageconfig->denyMessage, (char*) denymessage);
		config_denymessage = 1;
	}
}

void get_denyratemessage_from_config(){
	if(denyratemessage){
		responsemessageconfig->denyRateMessage = ngx_slab_alloc(shpool, strlen((char*)denyratemessage) + 1);
		if(!responsemessageconfig->denyRateMessage){
			return;
		}
		memset(responsemessageconfig->denyRateMessage, 0, strlen((char*)denyratemessage) + 1);
		strcpy(responsemessageconfig->denyRateMessage, (char*) denyratemessage);
		config_denyratemessage = 1;
	}
}

	static ngx_int_t
ngx_alpaca_client_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt        *h;
	ngx_http_core_main_conf_t  *cmcf;
	ngx_alpaca_client_main_conf_t *conf;

	conf = ngx_http_conf_get_module_main_conf(cf, ngx_alpaca_client_module);
	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	zookeeper_addr = conf->zookeeper_addr.data;
	local_ip = getLocalIP((char *)conf->interface.data);
	getVisitId(conf);
	allow_ua_empty = conf->allow_ua_empty;
	denymessage = conf->denymessage.data;
	denyratemessage = conf->denyratemessage.data;
	lua_filename = conf->lua_file.data;

	alpaca_log_open((char*)conf->log.data, (char*)conf->level.data);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_alpaca_client_handler;

	socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
	//return ngx_alpaca_client_init_zookeeper_shm(cf);
	return NGX_OK;
}

//static ngx_int_t
//ngx_alpaca_client_init_zone(ngx_shm_zone_t *shm_zone, void *data){
//	shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
//	push_event_num = ngx_slab_alloc(shpool, sizeof(unsigned long));
//	if(!push_event_num){
//		return NGX_ERROR;
//	}
//	policyconfig = ngx_slab_alloc(shpool, sizeof(PolicyConfig));
//	if(!policyconfig){
//		return NGX_ERROR;
//	}
//	responsemessageconfig = ngx_slab_alloc(shpool, sizeof(ResponseMessageConfig));
//	if(!responsemessageconfig){
//		return NGX_ERROR;
//	}
//	commonconfig = ngx_slab_alloc(shpool, sizeof(CommonConfig));
//	if(!commonconfig){
//		return NGX_ERROR;
//	}
//	switchconfig = ngx_slab_alloc(shpool, sizeof(SwitchConfig));
//	if(!switchconfig){
//		return NGX_ERROR;
//	}
//	get_denymessage_from_config();
//	get_denyratemessage_from_config();//need change
//
//	//	int pid = fork();
//	//	if(pid < 0){
//	//		alpaca_log_wirte(ALPACA_ERROR, "create process fail, when inited");
//	//	}
//	//	else if(pid == 0){
//	//		initConfigWatch(zookeeper_addr);
//	//		heartbeatcycle();
//	//	}
//
//	return NGX_OK;
//}

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

/*	static char *
	ngx_alpaca_client_merge_main_conf(ngx_conf_t *cf, void *parent, void *child)
	{
	printf("called:ngx_echo_merge_loc_conf\n");
	ngx_alpaca_client_main_conf_t *prev = parent;
	ngx_alpaca_client_main_conf_t *conf = child;

	ngx_conf_merge_str_value(conf->ecdata, prev->ecdata, 10);
	ngx_conf_merge_str_value(conf->zookeeper_addr, prev->zookeeper_addr, "localhost:2181");
	ngx_conf_merge_str_value(conf->visitId, prev->visitId, "_hc.v");
	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_ptr_value(conf->zh, prev->zh, NULL);
	return NGX_CONF_OK;
	}*/
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
	pbcf->mmap_dat_size = NGX_CONF_UNSET;

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


	initConfigWatch(zookeeper_addr);

	return NGX_OK;
}


	static ngx_int_t
ngx_proc_send_loop(ngx_cycle_t *cycle)
{
	ngx_log_error(NGX_LOG_ERR, cycle->log, 0, "hupeng test send loop");
	//heartbeatcycle();
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
	u_char             sa[NGX_SOCKADDRLEN], buf[256], *p;
	socklen_t          socklen;
	ngx_socket_t       s;
	ngx_connection_t  *lc;

	lc = ev->data;
	s = accept(lc->fd, (struct sockaddr *) sa, &socklen);
	if (s == -1) {
		return;
	}

	if (ngx_nonblocking(s) == -1) {
		goto finish;
	}

	p = ngx_sprintf(buf, "%s, %s, %d, %d, %d:%d:%d-%s",
			week[ngx_cached_tm->tm_wday],
			months[ngx_cached_tm->tm_mon],
			ngx_cached_tm->tm_mday, ngx_cached_tm->tm_year,
			ngx_cached_tm->tm_hour, ngx_cached_tm->tm_min,
			ngx_cached_tm->tm_sec, ngx_cached_tm->tm_zone);

	ngx_write_fd(s, buf, p - buf);


finish:
	ngx_close_socket(s);
}
