typedef struct CommonConfig{
	char* clientStatusUrl;
	char* clientEnableUrl;
	char* clientDisableUrl;
	char* clientValidateCodeUrl;
	char* serverRoot;
	char* serverBlockEventUrl;
	int* clientHeartbeatInterval;
}CommonConfig;

CommonConfig commonconfig;
