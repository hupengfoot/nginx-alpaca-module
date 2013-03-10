typedef struct {
	char * key;
	char * value;
}Pair;

typedef struct{
	Pair* list;
	int len;
}PairList;

typedef struct {
	char** list;
	int len;
}List;

typedef struct {
	Pair key;
	char* value;
}Triple;

typedef struct{
	Triple* list;
	int len;
}TripleList;

typedef struct{
	List* list;
	int len;
}ListList;

typedef struct {
	 List* volatile  acceptIPAddressPrefix;
	 List* volatile  acceptHttpMethod;
	 List* volatile  denyUserAgent;
	 List* volatile  denyUserAgentPrefix;
	 List* volatile  denyIPAddress;
	 List* volatile  denyIPAddressPrefix;
	 List* volatile  denyVistorID;
	 ListList* volatile denyUserAgentContainAnd;
	 TripleList* volatile denyIPVidRate;
	 PairList* volatile  denyIPVidRateStr;
	 PairList* volatile  denyVistorIDRate;
	 PairList* volatile  denyNOVisitorIDURL;
	 PairList* volatile  denyIPAddressRate;
}PolicyConfig;


PolicyConfig* policyconfig;
