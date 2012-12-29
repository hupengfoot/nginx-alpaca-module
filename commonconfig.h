typedef struct CommonConfig{
	volatile char* clientStatusUrl;
	volatile char* clientEnableUrl;
	volatile char* clientDisableUrl;
	volatile char* clientValidateCodeUrl;
	volatile int* clientHeartbeatInterval;
}CommonConfig;

CommonConfig commonconfig;
