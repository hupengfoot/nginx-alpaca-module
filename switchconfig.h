#include <stdbool.h>
typedef struct SwitchConfig{
	volatile int running;
	volatile int enable;
	volatile int pushBlockEvent;
	volatile int mount;
	volatile int blockByVid;
}SwitchConfig;

SwitchConfig switchconfig;

