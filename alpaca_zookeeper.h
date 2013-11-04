
#include <zookeeper/zookeeper.h>

#define ZOOKEEPERBUFSIZE 2048*100
#define ZOOKEEPERROUTE "/DP/CONFIG/"
//#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.acceptIPPrefix", "alpaca.policy.acceptHttpMethod", "alpaca.policy.denyUserAgent", "alpaca.policy.denyUserAgentPrefix", "alpaca.policy.denyIPAddressPrefix", "alpaca.policy.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.denyIPVidRate", "alpaca.policy.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.denyVisterID", "alpaca.policy.denyVisterIDRate"}

#define ZOOKEEPERWATCHKEYS {"alpaca.filter.enable", "alpaca.policy.withdomain.denyIPAddress", "alpaca.filter.pushBlockEvent", "alpaca.filter.mount", "alpaca.client.clientHeartbeatEnable","alpaca.filter.blockByVid", "alpaca.policy.withdomain.acceptIPPrefix", "alpaca.policy.withdomain.acceptHttpMethod", "alpaca.policy.withdomain.denyUserAgent", "alpaca.policy.withdomain.denyUserAgentPrefix", "alpaca.policy.withdomain.denyIPAddressPrefix", "alpaca.policy.withdomain.denyIPAddressRate", "alpaca.policy.denyUserAgentContainAnd", "alpaca.policy.withdomain.denyIPVidRate", "alpaca.policy.withdomain.denyNoVisitorIdURL.new", "alpaca.url.clientStatusUrl", "alpaca.url.clientEnableUrl", "alpaca.url.clientDisableUrl", "alpaca.url.clientValidateCodeUrl", "alpaca.client.heartbeat.interval", "alpaca.message.denyrate", "alpaca.url.serverRootUrl", "alpaca.url.serverBlockEventNotifyUrl", "alpaca.url.serverHeartbeatUrl","alpaca.filter.blockByVidOnly","alpaca.policy.withdomain.denyVisterID", "alpaca.policy.withdomain.denyVisterIDRate"}
#define ALPACA_MAX_PROCESS 1024

#define DEFAULTDENYMESSAGE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#2583ad;text-decoration:none;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}p{{text-align:center;}}sub{{display:block;margin:0;padding:0;color:#aaa;font-size:11px;text-align:right;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><p>对不起，您的访问存在某些问题。<br />如果您是正常访问，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，并附上以下信息：<br /><textarea rows=\"10\" cols=\"80\">${0}\r\n${1}\r\n${2}</textarea></p><sub>${0}</sub><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_agent\");</script></body></html>"
#define DEFAULT_CLIENT_URL_DISABLE "/dianping.firewall.client.disable"
#define DEFAULT_CLIENT_URL_ENABLE "/dianping.firewall.client.enable"
#define DEFAULT_CLIENT_HEARTBEAT_INTERVAL 180
#define DEFAULT_CLIENT_URL_STATUS "/dianping.firewall.client.status"
#define DEFAULT_CLIENT_URL_VALIDATECODE "/deny.code"
#define DEFAULT_SERVERROOT "http://192.168.26.48:8080"
#define DEFAULT_SERVER_URL_BLOCK_EVENT "/clientManagement/dianping.firewall.server.blockevent"
#define DEFAULT_SERVER_URL_HEARTBEAT "/clientManagement/dianping.firewall.server.heartbeat"
#define DEFAULTDENYRATE "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><title>提示_大众点评网</title><style type=\"text/css\">html{{background:#f7f7f7;}}body{{background:#fff;color:#333;font-family:\"MicrosoftYaHei\",\"微软雅黑\",Verdana,Arial;margin:2em auto 0 auto;width:700px;padding:1em 2em;-moz-border-radius:11px;-khtml-border-radius:11px;-webkit-border-radius:11px;border-radius:11px;border:1px solid #dfdfdf;}}a{{color:#ccc;}}a:hover{{color:#d54e21;}}h1{{border-bottom:1px solid #dadada;clear:both;color:#666;margin:5px 0 5px 0;padding:0;padding-bottom:1px;}}form{{padding:8px;font-size:14px;line-height:18px;text-align:center;}}form input{{font-size:20px;font-weight:bold;}}form input.i{{width:190px;}}p{{margin-bottom:30px;}}div{{margin-bottom:8px;}}p.c{{color:#ccc;}}</style></head><body><h1 id=\"logo\" style=\"text-align: center\"><img alt=\"dianping.com\" src=\"http://i1.dpfile.com/s/img/logo.gif\" /></h1><form method=\"post\" action=\"/validcode\"><p>对不起，你访问的太快了，请输入验证码后继续浏览：</p><div><img  id=\"code\" src=\"/deny.code\" alt=\"验证码\" /></div><div> <input name=\"vode\" class=\"i\" type=\"text\" /><input type=\"submit\" value=\" 提 交 \" /><input type=\"hidden\" name=\"referer\" value=\"hupeng\" /></div><p class=\"c\">如果您(${0})经常碰到此情况，请与<a href=\"mailto:spam@dianping.com\">spam@dianping.com</a>联系，我们会尽快处理。</p></form><script type=\"text/javascript\" src=\"http://i2.dpfile.com/s/res/ga.js\"></script><script type=\"text/javascript\">var pageTracker = _gat._getTracker(\"UA-464026-1\");pageTracker._initData();pageTracker._trackPageview(\"firewall_deny_rate\");</script></body></html>"
#define DEFAULT_DUMP_INFO_SIZE 1024*1024


typedef struct{
	ngx_socket_t        pipefd[2];
}alpaca_pipe_t;

int alpaca_worker_processes;

void get_zk_value(char* keyname, char* buffer, int buflen, int i);
void init_config_watch(u_char* zookeeper_addr);
void register_zk_value();
char* dumpStatus();
void watcher(zhandle_t *zzh, int type, int state, const char *path, void *watcherCtx);
void set_default();
void set_digit(char* buf, int volatile* key);
void set_int(char* buf, int volatile* key);
void set_string(char* buf, char* volatile* key);
