
#ifndef _POLICYCONFIG_H_
#include "policyconfig.h"
#define _POLICYCONFIG_H_
#endif

char *url_encode(char const *s, int len, int *new_length);
int pairUrlEncode(Pair* httpParams, char* out, int len);
