
#include <ngx_http.h>

#define MD5_LEN 33 

char* getmd5(const char* data);
char* getmd5frompool(char* buf, const char* data);
char* getmd5fromngxpool(ngx_http_request_t *r, const char* data);
