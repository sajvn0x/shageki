#include "core/memory.h"
#include "renderer/renderer.h"
#include "vk_device.h"
#include "vk_types.h"

bool vulkan_renderer_init(RendererState* state, GLFWwindow* window) {
    state->backend_state =
        memory_allocate(sizeof(VulkanContext), MEMORY_TAG_VULKAN);

    VulkanContext* context = (VulkanContext*)state->backend_state;
    context->allocator = 0;

    if (!vulkan_device_create(context, window)) {
        return false;
    }

    return true;
}

void vulkan_renderer_destroy(RendererState state) {
    VulkanContext* context = (VulkanContext*)state.backend_state;

    vulkan_device_destroy(context);

    memory_free(state.backend_state, sizeof(VulkanContext), MEMORY_TAG_VULKAN);
    state.backend_state = 0;
}

bool vulkan_renderer_begin_frame(RendererState* state) { return true; }

void vulkan_renderer_end_frame(RendererState* state) {}
