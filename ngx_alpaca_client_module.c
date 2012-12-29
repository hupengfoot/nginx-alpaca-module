
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
#define DEFAULTDENYRATE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#ccc;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}form{{padding:8px;font-size:14px;line-height:18px;text-align:center;}}form input{{font-size:20px;font-weight:bold;}}form input.i{{width:190px;}}p{{margin-bottom:30px;}}div{{margin-bottom:8px;}}p.c{{color:#ccc;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><form method=\"post\" action=\"/validcode\"><p>对不起，你访问的太快了，请输入验证码后继续浏览：</p><div><img  id=\"code\" src=\"/deny.code\" alt=\"验证码\" /></div><div> <input name=\"vode\" class=\"i\" type=\"text\" /><input type=\"submit\" value=\" 提 交 \" /><input type=\"hidden\" name=\"referer\" value=\"hupeng\" /></div><p class=\"c\">如果您(${0})经常碰到此情况，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，我们会尽快处理。</p></form><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_rate\");</script></body></html>"

static ngx_int_t ngx_alpaca_client_handler(ngx_http_request_t *r);
static ngx_int_t ngx_alpaca_client_init(ngx_conf_t *cf);
static void *ngx_alpaca_client_create_loc_conf(ngx_conf_t *cf);
static char *ngx_alpaca_client_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);


static ngx_command_t  ngx_alpaca_client_commands[] = {

	{ ngx_string("alpaca"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_alpaca_client_loc_conf_t, enable),
		NULL },
	{ ngx_string("alpaca_zoo"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_alpaca_client_loc_conf_t, zookeeper_addr),
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
	ngx_int_t                  rc;
	ngx_chain_t                *out;

	ahlf = ngx_http_get_module_loc_conf(r, ngx_alpaca_client_module);
	if(ahlf->zh == NULL){
		init(ahlf, r);
	}
	if(ahlf->enable){
		if(procrequest(r) == 0){
			return NGX_DECLINED;
		}
		r->headers_out.status = NGX_HTTP_OK;
		r->headers_out.content_length_n = strlen(policyconfig.denyIPAddress[0]);
		//r->headers_out.content_length_n = ahlf->ecdata.len - 1;
		rc = ngx_http_send_header(r);
		if(rc == 0){
		}
		ngx_buf_t    *b;  
		b = ngx_calloc_buf(r->pool);  
		if (b == NULL) {  
			return NGX_ERROR;  
		} 
		char *resbody = malloc(r->headers_out.content_length_n);
		strcpy(resbody,policyconfig.denyIPAddress[0]);
		b->pos = (u_char *) resbody;
	      	//b->pos = ahlf->ecdata.data;	
		b->last = b->pos + strlen(policyconfig.denyIPAddress[0]);  
		//b->last = b->pos + ahlf->ecdata.len - 1;
		b->memory = 1;  
		b->last_buf = 1;  
		out = ngx_alloc_chain_link(r->pool);  
		if (out == NULL)  
			return NGX_ERROR;  
		out->buf = b;  
		out->next = NULL;  
		out->buf->last_buf = 1;  
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
ngx_alpaca_client_create_loc_conf(ngx_conf_t *cf)
{
	printf("called:ngx_alpaca_client_create_loc_conf\n");
	ngx_alpaca_client_loc_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_alpaca_client_loc_conf_t));
	if (conf == NULL) {
		return NGX_CONF_ERROR;
	}

	conf->ecdata.len = 0;
	conf->ecdata.data = NULL;
	conf->zookeeper_addr.len = 0;
	conf->zookeeper_addr.data = NULL;
	conf->zh = NGX_CONF_UNSET_PTR;
	conf->enable = NGX_CONF_UNSET;
	return conf;
}
	static char *
ngx_alpaca_client_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	printf("called:ngx_echo_merge_loc_conf\n");
	ngx_alpaca_client_loc_conf_t *prev = parent;
	ngx_alpaca_client_loc_conf_t *conf = child;

	/*conf->zh = zookeeper_init("localhost:2181", watcher, 10000, 0, conf, 0);
	  char *buffer = malloc(512);
	  struct Stat stat;
	  int buflen= 512;
	  zoo_get(conf->zh, "/hupeng", 1, buffer, &buflen, &stat);
	  conf->ecdata.data = (u_char*) buffer;
	  conf->ecdata.len = strlen(buffer);*/

	ngx_conf_merge_str_value(conf->ecdata, prev->ecdata, 10);
	ngx_conf_merge_str_value(conf->zookeeper_addr, prev->zookeeper_addr, "localhost:2181");
	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_ptr_value(conf->zh, prev->zh, NULL);
	return NGX_CONF_OK;
}

