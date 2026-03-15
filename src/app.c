#include "app.h"

#include "core/logger.h"
#include "renderer/vulkan/vulkan_backend.h"

bool app_initialize(App* out_app) {
	// window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	out_app->window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, NULL, NULL);

	// renderer
	// For now, only going to support Vulkan
	out_app->renderer.renderer_init = vulkan_renderer_init;
	out_app->renderer.renderer_destroy = vulkan_renderer_destroy;
	out_app->renderer.renderer_begin_frame = vulkan_renderer_begin_frame;
	out_app->renderer.renderer_end_frame = vulkan_renderer_end_frame;

	// initialize renderer
	if (!out_app->renderer.renderer_init(&out_app->renderer_state, out_app->window)) {
		LOG_ERROR("Failed to initialize the Renderer");
		return false;
	}

	return true;
}

void app_run_loop(App* app) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
	}
}

void app_shutdown(App app) {
	app.renderer.renderer_destroy(app.renderer_state);

    glfwDestroyWindow(app.window);
    glfwTerminate();
}

