#include "event.h"
#include "sat.h"

int main(int argc, char *argv[])
{
	int ret;

	event_init();
	if (!(ret = sat_init(argc, argv)))
		event_loop();
	event_uninit();
	return ret;
}

