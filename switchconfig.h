typedef struct SwitchConfig{
	  int volatile  running;
	  char* volatile string_running;
	  int volatile  enable;
	  char* volatile string_enable;
	  int volatile  pushBlockEvent;
	  char* volatile string_pushBlockEvent;
	  int volatile  mount;
	  char* volatile string_mount;
	  int volatile  clientHeartbeatEnable;
	  char* volatile string_clientHeartbeatEnable;
	  int volatile  blockByVid;
	  char* volatile string_blockByVid;
	  int volatile  blockByVidOnly;
	  char* volatile string_blockByVidOnly;
}SwitchConfig;

SwitchConfig* switchconfig;

