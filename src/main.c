#include <GLFW/glfw3.h>

#include "app.h"
#include "core/logger.h"

int main(void) {
	App app;
	if(!app_initialize(&app)) {
		LOG_ERROR("Failed to initialize the App");
		return -1;
	}

	// main loop
	app_run_loop(&app);

	app_shutdown(app);

	return 0;
}

