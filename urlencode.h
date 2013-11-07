#include "blockrequestqueue.h"

char *url_encode(char const *s, int len, int *new_length);
int pairUrlEncode(Pair* httpParams, char* out, int len);
