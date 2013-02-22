#define ALPACA_DEBUG 0
#define ALPACA_INFO 1
#define ALPACA_WARN 2
#define ALPACA_ERROR 3
#define DEFAULT_ALPACA_LOG_LEVEL 2


typedef struct{
	int log_level;
	int fd;	
}alpaca_log_t;

int alpaca_log_open(const char * name);
int alpaca_log_append(int fd, char* data);
