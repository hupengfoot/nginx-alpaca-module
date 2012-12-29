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
	int acceptIPAddressPrefix_len;
	char** acceptHttpMethod;
	int acceptHttpMethod_len;
	char** denyUserAgent;
	int denyUserAgent_len;
	char** denyUserAgentPrefix;
	int denyUserAgentPrefix_len;
	char** denyIPAddress;
	int denyIPAddress_len;
	char** denyIPAddressPrefix;
	int denyIPAddressPrefix_len;
	Pair** denyIPAddressRate;
	int denyIPAddressRate_len;
	List** denyUserAgentContainAnd;
	int denyUserAgentContainAnd_len;
	Triple** denyIPVidRate;
	int denyIPVidRate_len;
	Pair** denyIPVidRateStr;
	int denyIPVidRateStr_len;
	Pair** denyNOVistorIDURL;
	int denyNOVistorIDURL_len;
}PolicyConfig;

PolicyConfig policyconfig;
