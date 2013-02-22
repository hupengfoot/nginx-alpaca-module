#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


int alpaca_log_open(const char * name){
	return open(name, O_CREAT|O_WRONLY|O_APPEND, 0644);
}

int alpaca_log_append(int fd, char* data){
	return write(fd, data, strlen(data));
}
