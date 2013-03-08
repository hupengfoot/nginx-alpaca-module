#ifndef _POLICYCONFIG_H_
#include "policyconfig.h"
#define _POLICYCONFIG_H_
#endif
int startWithIgnoreCaseContains(char* target, List* collection);
int ignoreCaseContains(char* target, List* collection, int len);
int contains(char* target, List* collection, int len);
int ignoreCaseContainAll(char* target, ListList* collection_list);
