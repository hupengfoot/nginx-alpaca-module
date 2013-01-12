typedef struct {
	char * key;
	char * value;
}Pair;

typedef struct{
	Pair** list;
	int len;
}PairList;

typedef struct {
	char** list;
	int len;
}List;

typedef struct {
	Pair* key;
	char* value;
}Triple;

typedef struct{
	Triple** list;
	int len;
}TripleList;

typedef struct{
	List** list;
	int len;
}ListList;

typedef struct {
	List* acceptIPAddressPrefix;
	List* acceptHttpMethod;
	List* denyUserAgent;
	List* denyUserAgentPrefix;
	List* denyIPAddress;
	List* denyIPAddressPrefix;
	PairList* denyIPAddressRate;
	ListList* denyUserAgentContainAnd;
	TripleList* denyIPVidRate;
	PairList* denyIPVidRateStr;
	PairList* denyNOVisitorIDURL;
}PolicyConfig;

PolicyConfig policyconfig;
