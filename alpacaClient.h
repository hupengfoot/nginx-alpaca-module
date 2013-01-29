#include <zookeeper/zookeeper.h>
#include <ngx_core.h>


#define CONTEXTSTATUSNEEDNOTRESPONSE -1
#define CONTEXTSTATUSNEEDRESPONSE 0

typedef struct{
	ngx_flag_t zh;
	ngx_str_t zookeeper_addr;
	ngx_str_t ecdata;
	ngx_str_t visitId;
	ngx_flag_t enable;
} ngx_alpaca_client_main_conf_t;

enum status {
	DENY_VID = 407,
	DENY_VIDRATE = 408,
	DENY_HTTPMETHOD = 406,
       	DENY_USERAGENT = 401,
       	DENY_IP = 403,
       	DENY_IPRATE = 402,
       	DENY_IPVIDRATE = 404,
       	DENY_NOVID = 405,
       	SUCCESS = 200,
       	PASS = 201,
       	VALIDATECODE = 100,
       	SHOWSTATUS = 202
};

typedef struct{
	u_char* clientIP;
	size_t clientIP_len;
	u_char* userAgent;
	size_t userAgent_len;
	u_char* httpMethod;
	size_t httpMethod_len;
	u_char* rawUrl;
	size_t rawUrl_len;
	enum status status;
	u_char* visitId;
	size_t visitId_len;
}Context;

void init(ngx_alpaca_client_main_conf_t *aclc, ngx_http_request_t *r);
void initConfigWatch(ngx_alpaca_client_main_conf_t *aclc, ngx_http_request_t *r);
int doFilter(ngx_http_request_t *r, ngx_chain_t **out);




