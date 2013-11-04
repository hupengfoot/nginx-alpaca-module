
typedef struct {
	char* volatile  acceptIPAddressPrefix;
	char* volatile  acceptHttpMethod;
	char* volatile  denyUserAgent;
	char* volatile  denyUserAgentPrefix;
	char* volatile  denyIPAddress;
	char* volatile  denyIPAddressPrefix;
	char* volatile  denyVistorID;
	char* volatile denyUserAgentContainAnd;
	char* volatile denyIPVidRate;
	char* volatile  denyIPVidRateStr;
	char* volatile  denyVistorIDRate;
	char* volatile  denyNOVisitorIDURL;
	char* volatile  denyIPAddressRate;
}PolicyConfig;


PolicyConfig* policyconfig;
