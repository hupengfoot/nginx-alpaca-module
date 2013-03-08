#include <malloc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "alpaca_log.h"

char* getLocalIP(){
	int fd;
	struct ifreq ifr;
	struct sockaddr_in* sin;
	char *ip;
	ip = (char*)malloc(32);
	if(!ip){
		alpaca_log_wirte(ALPACA_WARN, "malloc fail, when get local ip");
		return NULL;
	}
	memset(ip, 0, 32);
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	memset(&ifr, 0x00, sizeof(ifr));
	strcpy(ifr.ifr_name, "eth0");
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	sin = (struct sockaddr_in* )&ifr.ifr_addr;
	ip = (char *)inet_ntoa(sin->sin_addr);
	return ip;
}
