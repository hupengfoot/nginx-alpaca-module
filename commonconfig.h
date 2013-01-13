typedef struct CommonConfig{
	char* clientStatusUrl;
	char* clientEnableUrl;
	char* clientDisableUrl;
	char* clientValidateCodeUrl;
	char* serverRoot;
	char* serverBlockEventUrl;
	char* serverHeartbeatUrl;
	int* clientHeartbeatInterval;
}CommonConfig;

CommonConfig commonconfig;
