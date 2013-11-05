
#define ALPACA_CLIENT_VERSION "1.0.0"

#define TOKEN_KEY "alpaca-firewall-token"

#define POOL_SIZE 1024*10

#define DEFAULT_VISIT_ID  "_hc.v"

#define DENYMESSAGEMAXLENTH 8192

#define DEFAULT_BLOCK_MAX_LENTH 4096

#define DEFAULT_CLIENT_HEARTBEAT_ENABLE 0

#define EXPIRETIME 180

#ifdef __x86_64__
#define U_CHAR long
#elif __i386__
#define U_CHAR int
#endif

#define DEFAULT_PIPE_SIZE 4096

#define DEFAULT_ALPACA_PIPE_BUF 1024*100

#define DEFAULT_ALPACA_KEY_MAX_LEN 100
