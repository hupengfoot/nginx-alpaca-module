#include "ngx_http.h"

char* getmd5(const char* data);
char* getmd5frompool(alpaca_memory_pool* pool, const char* data);
char* getmd5fromngxpool(ngx_http_request_t *r, const char* data);
