#include <stdbool.h>
typedef struct SwitchConfig{
	  int volatile  running;
	  int volatile  enable;
	  int volatile  pushBlockEvent;
	  int volatile  mount;
	  int volatile  clientHeartbeatEnable;
	  int volatile  blockByVid;
	  int volatile  blockByVidOnly;
}SwitchConfig;

SwitchConfig* switchconfig;

