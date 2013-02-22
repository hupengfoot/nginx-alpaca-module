
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <zookeeper/zookeeper.h>
#include "alpacaClient.h"
#include "policyconfig.h"

static ngx_int_t ngx_alpaca_client_handler(ngx_http_request_t *r);
static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void *ngx_alpaca_client_create_main_conf(ngx_conf_t *cf);
//static char *ngx_alpaca_client_merge_main_conf(ngx_conf_t *cf, void *parent, void *child);


static ngx_command_t  ngx_alpaca_client_commands[] = {

	{ ngx_string("alpaca"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
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
	if(ahlf->zh == NGX_CONF_UNSET){
		init(ahlf, r);
	}
	if(ahlf->enable){
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



	static ngx_int_t
ngx_alpaca_client_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt        *h;
	ngx_http_core_main_conf_t  *cmcf;
	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_alpaca_client_handler;
	return NGX_OK;
}

	static void *
ngx_alpaca_client_create_main_conf(ngx_conf_t *cf)
{
	printf("called:ngx_alpaca_client_create_main_conf\n");
	ngx_alpaca_client_main_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_alpaca_client_main_conf_t));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}
	conf->zh = NGX_CONF_UNSET;
	conf->ecdata.len = 0;
	conf->ecdata.data = NULL;
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

