#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "alpaca_log.h"


static alpaca_log_t alpaca_log;

char* get_now_time(){
	time_t timep;
	time(&timep);
	return ctime(&timep);
}
int get_alpaca_log_level(char* level){
	if(strcasecmp(level, "DEBUG") == 0){
		return 0;
	}
	else if(strcasecmp(level, "INFO") == 0){
		return 1;
	}
	else if(strcasecmp(level, "WARN") == 0){
		return 2;
	}
	else if(strcasecmp(level, "ERROR") == 0){
		return 3;
	}
	else{
		return 2;
	}
}

int alpaca_log_open(const char * name, char* level){
	alpaca_log.fd = open(name, O_CREAT|O_WRONLY|O_APPEND, 0644);
	if(!level){
		alpaca_log.log_level = DEFAULT_ALPACA_LOG_LEVEL;
	}
	else{
		alpaca_log.log_level = get_alpaca_log_level(level);
	}
	return alpaca_log.fd;
}

int alpaca_log_append(int fd, char* data){
	return write(fd, data, strlen(data));
}

int alpaca_log_wirte(int level, char*data){
	if(level < alpaca_log.log_level){
		return -1;
	}
	if(alpaca_log.fd < 0){
		return -1;
	}
	char msg[DEFAULT_MAX_LOG_MSG];
	memset(msg, 0, DEFAULT_MAX_LOG_MSG);
	char* nowtime = get_now_time();
	if(!nowtime){
		return -1;
	}
	strncpy(msg, nowtime, strlen(nowtime) - 1);
	strcat(msg, "        ");
	if(level == 0){
		strcat(msg, "[DEBUG]");
	}
	else if(level == 1){
		strcat(msg, "[INFO]");
	}
	else if(level == 2){
		strcat(msg, "[WARN]");
	}
	else if(level == 3){
		strcat(msg, "[ERROR]");
	}
	else{
	}
	strcat(msg, "        ");
	strcat(msg, data);
	strcat(msg, "\n");
	return alpaca_log_append(alpaca_log.fd, msg);
}
