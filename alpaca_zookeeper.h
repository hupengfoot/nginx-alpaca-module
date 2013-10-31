
#include <zookeeper/zookeeper.h>

#include "cJSON.h"

#define ZOOKEEPERBUFSIZE 2048*100
#define ZOOKEEPERROUTE "/DP/CONFIG/"
//#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.acceptIPPrefix", "alpaca.policy.acceptHttpMethod", "alpaca.policy.denyUserAgent", "alpaca.policy.denyUserAgentPrefix", "alpaca.policy.denyIPAddressPrefix", "alpaca.policy.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.denyIPVidRate", "alpaca.policy.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.denyVisterID", "alpaca.policy.denyVisterIDRate"}

#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.withdomain.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.withdomain.acceptIPPrefix", "alpaca.policy.withdomain.acceptHttpMethod", "alpaca.policy.withdomain.denyUserAgent", "alpaca.policy.withdomain.denyUserAgentPrefix", "alpaca.policy.withdomain.denyIPAddressPrefix", "alpaca.policy.withdomain.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.withdomain.denyIPVidRate", "alpaca.policy.withdomain.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.withdomain.denyVisterID", "alpaca.policy.withdomain.denyVisterIDRate"}
#define ALPACA_MAX_PROCESS 1024

typedef struct{
	ngx_socket_t        pipefd[2];
}alpaca_pipe_t;

int alpaca_worker_processes;

void get_zk_value(char* keyname, char* buffer, int buflen, int i);
void init_config_watch(u_char* zookeeper_addr);
void register_zk_value();
cJSON* dumpStatus();
void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
void set_default();
void set_digit(char* buf, int volatile* key);
void set_int(char* buf, int volatile* key);
void set_string(char* buf, char* volatile* key);
