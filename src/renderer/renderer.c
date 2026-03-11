#include "renderer.h"

#include <GLFW/glfw3.h>
#include <assert.h>
#include <string.h>

#include "core/error.h"

Renderer* renderer_init(GLFWwindow* window) {
    LOG_INFO("RENDERER: initialize\n");

    Renderer* renderer = malloc(sizeof(Renderer));
    if (!renderer) {
        LOG_ERROR("RENDERER: failed to allocate renderer\n");
        return NULL;
    }
    memset(renderer, 0, sizeof(Renderer));

    // Core initialization
    renderer->core = core_init(window);
    if (!renderer->core) {
        LOG_ERROR("RENDERER: core initialization failed\n");
        free(renderer);
        return NULL;
    }

    // Swapchain initialization
    if (swapchain_init(renderer, window) != APP_SUCCESS) {
        LOG_ERROR("RENDERER: swapchain initialization failed\n");
        core_destroy(renderer->core);
        free(renderer->core);
        free(renderer);
        return NULL;
    }

    // Frame data initialization
    if (frame_data_create(renderer) != APP_SUCCESS) {
        LOG_ERROR("RENDERER: frame data initialization failed\n");
        swapchain_destroy(renderer->swapchain);
        free(renderer->swapchain);
        core_destroy(renderer->core);
        free(renderer->core);
        free(renderer);
        return NULL;
    }

    assert(renderer->core);
    assert(renderer->swapchain);
    assert(renderer->frame);

    LOG_INFO("RENDERER: initialized\n");
    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    if (!renderer) return;
    LOG_INFO("RENDERER: destructing\n");

    if (renderer->frame) {
        frame_data_destroy(renderer->frame, renderer->core->device);
        free(renderer->frame);
        renderer->frame = NULL;
    }

    if (renderer->swapchain) {
        swapchain_destroy(renderer->swapchain);
        free(renderer->swapchain);
        renderer->swapchain = NULL;
    }

    if (renderer->core) {
        core_destroy(renderer->core);
        free(renderer->core);
        renderer->core = NULL;
    }

    LOG_INFO("RENDERER: destructed\n");
}
