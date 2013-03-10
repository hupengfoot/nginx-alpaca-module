typedef struct CommonConfig{
	char* volatile  clientStatusUrl;
	char* volatile  clientEnableUrl;
	char* volatile  clientDisableUrl;
	char* volatile  clientValidateCodeUrl;
	char* volatile  serverRoot;
	char* volatile  serverBlockEventUrl;
	char* volatile  serverHeartbeatUrl;
	int volatile clientHeartbeatInterval;
}CommonConfig;

CommonConfig* commonconfig;
