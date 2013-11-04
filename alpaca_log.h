#define ALPACA_DEBUG 0
#define ALPACA_INFO 1
#define ALPACA_WARN 2
#define ALPACA_ERROR 3
#define DEFAULT_ALPACA_LOG_LEVEL 1

#define DEFAULT_MAX_LOG_MSG 2048

typedef struct{
	int log_level;
	int fd;	
}alpaca_log_t;

int alpaca_log_open(const char * name, char* level);
int alpaca_log_wirte(int level, char*data);
