typedef struct {
	char * key;
	char * value;
}Pair;

typedef struct {
	char** list;
}List;

typedef struct {
	Pair* key;
	char* value;
}Triple;

typedef struct {
	char** acceptIPAddressPrefix;
	char** acceptHttpMethod;
	char** denyUserAgent;
	char** denyUserAgentPrefix;
	char** denyIPAddress;
	char** denyIPAddressPrefix;
	Pair** denyIPAddressRate;
	List** denyUserAgentContainAnd;
	Triple** denyIPVidRate;
	Pair** denyIPVidRateStr;
	Pair** denyNOVistorIDURL;
}PolicyConfig;

PolicyConfig policyconfig;
