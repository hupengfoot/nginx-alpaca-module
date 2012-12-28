#include <zookeeper/zookeeper.h>
#include <ngx_core.h>

typedef struct {
	zhandle_t *zh;
	ngx_str_t zookeeper_addr;
	ngx_str_t ecdata;
	ngx_flag_t enable;
} ngx_alpaca_client_loc_conf_t;

void init(ngx_alpaca_client_loc_conf_t *aclc, ngx_http_request_t *r);
void initConfigWatch(ngx_alpaca_client_loc_conf_t *aclc, ngx_http_request_t *r);
