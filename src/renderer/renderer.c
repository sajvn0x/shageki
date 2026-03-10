#include "renderer.h"

#include <GLFW/glfw3.h>
#include <string.h>

#include "core/error.h"

Renderer* renderer_init(GLFWwindow* window) {
    LOG_INFO("RENDERER: initialize\n");

    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        LOG_ERROR("RENDERER_CORE: Failed to create\n");
        return NULL;
    }
    renderer = memset(renderer, 0, sizeof(Renderer));

    Core* core = core_init(window);
    if (!core) {
        LOG_ERROR("RENDERER_CORE: Failed to create\n");
        return NULL;
    }
    renderer->core = core;

    AppResult swapchain_result = swapchain_init(renderer, window);
    if (swapchain_result != APP_SUCCESS) return NULL;

    LOG_INFO("RENDERER: initialized\n");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    LOG_INFO("RENDERER: destructing\n");

    if (renderer->swapchain) {
        swapchain_destroy(renderer->swapchain);
        free(renderer->swapchain);
    }

    if (renderer->core) {
        core_destroy(renderer->core);
        free(renderer->core);
    }

    LOG_INFO("RENDERER: destructed\n");
}
