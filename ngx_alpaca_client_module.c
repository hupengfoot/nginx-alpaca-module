

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

#define ZOOKEEPER_SHM_SIZE 10*1024*1024


extern ngx_slab_pool_t* shpool;
static int inited;
char* local_ip;
char* visitId;
static u_char* zookeeper_addr;
static ngx_int_t ngx_alpaca_client_handler(ngx_http_request_t *r);
static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void *ngx_alpaca_client_create_main_conf(ngx_conf_t *cf);
char *ngx_alpaca_client_init_conf(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_alpaca_client_init_zone(ngx_shm_zone_t *shm_zone, void *data);
void getVisitId(ngx_alpaca_client_main_conf_t *aclc);
//static char *ngx_alpaca_client_merge_main_conf(ngx_conf_t *cf, void *parent, void *child);


static ngx_command_t  ngx_alpaca_client_commands[] = {

	{ ngx_string("alpaca"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, enable),
		NULL },
	{ ngx_string("alpaca_zk"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, zookeeper_addr),
		NULL },
	{ ngx_string("alpaca_vid"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, visitId),
		NULL },
	{ ngx_string("alpaca_log"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, log),
		NULL },
	{ ngx_string("alpaca_log_level"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_MAIN_CONF_OFFSET,
		offsetof(ngx_alpaca_client_main_conf_t, level),
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
	NULL,                                  /* init process */
	NULL,                                  /* init thread */
	NULL,                                  /* exit thread */
	NULL,                                  /* exit process */
	NULL,                                  /* exit master */
	NGX_MODULE_V1_PADDING
};


	static ngx_int_t
ngx_alpaca_client_handler(ngx_http_request_t *r)
{
	ngx_alpaca_client_main_conf_t *ahlf;
	ngx_int_t                  rc;
	ngx_chain_t                *out = NULL;

	ahlf = ngx_http_get_module_main_conf(r, ngx_alpaca_client_module);
	if(!inited){ //TODO 
		init(ahlf, r);
		inited = 1;
	}
	if(ahlf->enable == 1){ //TODO reset
		if(doFilter(r, &out) == CONTEXTSTATUSNEEDNOTRESPONSE){
			return NGX_DECLINED;
		}
		rc = ngx_http_send_header(r);
		if(rc == NGX_ERROR){
			return rc;
		}
		return ngx_http_output_filter(r, out);
	}else{
		return NGX_DECLINED;
	}
}

static ngx_int_t ngx_alpaca_client_init_zookeeper_shm(ngx_conf_t *cf){
	ngx_str_t name;
	name.data = (u_char*) "zookeeper_shm";
	name.len = strlen("zookeeper_shm");
	size_t size;
	size = ZOOKEEPER_SHM_SIZE;
    	ngx_shm_zone_t            *shm_zone;
    	shm_zone = ngx_shared_memory_add(cf, &name, size,
                                     &ngx_alpaca_client_module);
    	if (shm_zone == NULL) {
        	return NGX_ERROR;
    	}
	shm_zone->init = ngx_alpaca_client_init_zone;
	shm_zone->data = name.data;//TODO how to set this value
	return NGX_OK;
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
	local_ip = getLocalIP();
    	getVisitId(conf);
	
	alpaca_log_open((char*)conf->log.data, (char*)conf->level.data);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_alpaca_client_handler;

	return ngx_alpaca_client_init_zookeeper_shm(cf);
}

static ngx_int_t
ngx_alpaca_client_init_zone(ngx_shm_zone_t *shm_zone, void *data){
	//strcpy((char*)shm_zone->shm.addr, "hupeng best");
	shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;
	policyconfig = ngx_slab_alloc(shpool, sizeof(PolicyConfig));
	if(!policyconfig){
		return NGX_ERROR;
	}
	responsemessageconfig = ngx_slab_alloc(shpool, sizeof(ResponseMessageConfig));
	if(!responsemessageconfig){
		return NGX_ERROR;
	}
	commonconfig = ngx_slab_alloc(shpool, sizeof(CommonConfig));
	if(!commonconfig){
		return NGX_ERROR;
	}
	switchconfig = ngx_slab_alloc(shpool, sizeof(SwitchConfig));
	if(!switchconfig){
		return NGX_ERROR;
	}

	int pid = fork();
	if(pid < 0){
	//TODO
	}
	else if(pid == 0){
		initConfigWatch(zookeeper_addr);
		heartbeatcycle();
	}
	else{
		//TODO
	}

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
	conf->log.len = 0;
	conf->log.data = NULL;
	conf->level.len = 0;
	conf->level.data = NULL;
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
	visitId = malloc(aclc->visitId.len + 1);
	if(visitId == NULL){
		alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get visitid");
		return;
	}
	memset(visitId, 0, aclc->visitId.len + 1);
	strcpy(visitId, (char*)aclc->visitId.data);
}
