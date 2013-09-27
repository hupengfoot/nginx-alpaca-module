#include <ngx_config.h>
#include <ngx_core.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#include "alpaca_log.h"

#define DEFAULT_INTERFACE_NAME "eth0" 

char* getLocalIP(char* interfacename)
{
	char *ip;
	char *ifn;

	if(interfacename == NULL){
		ifn = malloc(strlen(DEFAULT_INTERFACE_NAME) + 1);
		if(ifn == NULL){
			alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get local ip");
			return NULL;
		}
		strcpy(ifn, DEFAULT_INTERFACE_NAME);
	}
	else{
		ifn = interfacename;
	}

	struct sockaddr_in *sin = NULL;
	struct ifaddrs *ifa = NULL, *ifList;

	if (getifaddrs(&ifList) < 0) return -1;
	for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
	{
		if(ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, ifn) == 0)
		{
			sin = (struct sockaddr_in *)ifa->ifa_addr;
			ip = (char *)inet_ntoa(sin->sin_addr);
			return ip;
		}
	}
	freeifaddrs(ifList);
	return NULL;
}
