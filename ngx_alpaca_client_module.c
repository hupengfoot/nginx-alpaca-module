
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>





static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void* ngx_alpaca_client_create_loc_conf(ngx_conf_t *cf);
static char* ngx_alpaca_client_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


typedef struct {
    ngx_str_t ecdata;
    ngx_flag_t enable;
} ngx_alpaca_client_loc_conf_t;

static ngx_command_t  ngx_alpaca_client_commands[] = {

    { ngx_string("alpaca"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_alpaca_client_loc_conf_t, enable),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_alpaca_client_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_alpaca_client_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_alpaca_client_create_loc_conf,       /* create location configuration */
    ngx_alpaca_client_merge_loc_conf        /* merge location configuration */
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
    ngx_alpaca_client_loc_conf_t *ahlf;

    ahlf = ngx_http_get_module_loc_conf(r, ngx_alpaca_client_module);
    if(ahlf->enable){
	 return NGX_HTTP_SERVICE_UNAVAILABLE;
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

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_PREACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_alpaca_client_handler;

    return NGX_OK;
}

static void *
ngx_alpaca_client_create_loc_conf(ngx_conf_t *cf)
{
    printf("called:ngx_alpaca_client_create_loc_conf\n");
    ngx_alpaca_client_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_alpaca_client_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->ecdata.len=0;
    conf->ecdata.data=NULL;
    conf->enable = NGX_CONF_UNSET;
    return conf;
}
static char *
ngx_alpaca_client_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    printf("called:ngx_echo_merge_loc_conf\n");
    ngx_alpaca_client_loc_conf_t *prev = parent;
    ngx_alpaca_client_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->ecdata, prev->ecdata, 10);
    ngx_conf_merge_value(conf->enable, prev->enable, 0);
/**
    if(conf->enable)
        ngx_echo_init(conf);
        */
    return NGX_CONF_OK;
}

