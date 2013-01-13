#include <stdbool.h>
typedef struct SwitchConfig{
	  int *running;
	  int *enable;
	  int *pushBlockEvent;
	  int *mount;
	  int *clientHeartbeatEnable;
	  int *blockByVid;
	  int *blockByVidOnly;
}SwitchConfig;

SwitchConfig switchconfig;

