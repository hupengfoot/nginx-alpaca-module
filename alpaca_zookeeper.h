
#include <zookeeper/zookeeper.h>

#include "cJSON.h"

#define ZOOKEEPERBUFSIZE 2048*100
#define ZOOKEEPERROUTE "/DP/CONFIG/"
//#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.acceptIPPrefix", "alpaca.policy.acceptHttpMethod", "alpaca.policy.denyUserAgent", "alpaca.policy.denyUserAgentPrefix", "alpaca.policy.denyIPAddressPrefix", "alpaca.policy.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.denyIPVidRate", "alpaca.policy.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.denyVisterID", "alpaca.policy.denyVisterIDRate"}

#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.withdomain.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.withdomain.acceptIPPrefix", "alpaca.policy.withdomain.acceptHttpMethod", "alpaca.policy.withdomain.denyUserAgent", "alpaca.policy.withdomain.denyUserAgentPrefix", "alpaca.policy.withdomain.denyIPAddressPrefix", "alpaca.policy.withdomain.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.withdomain.denyIPVidRate", "alpaca.policy.withdomain.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.withdomain.denyVisterID", "alpaca.policy.withdomain.denyVisterIDRate"}
void get_zk_value(char* keyname, char* buffer, int buflen, int i);
void initConfigWatch(u_char* zookeeper_addr);
cJSON* dumpStatus();
void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
void setDefault();
