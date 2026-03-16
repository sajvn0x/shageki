#include "core/logger.h"
#include "core/memory.h"
#include "renderer/renderer.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "vk_types.h"

i32 find_memory_index(VulkanContext* context, u32 type_filter,
                      u32 property_flags);

bool vulkan_renderer_init(RendererState* state, GLFWwindow* window) {
    state->backend_state =
        memory_allocate(sizeof(VulkanContext), MEMORY_TAG_VULKAN);

    VulkanContext* context = (VulkanContext*)state->backend_state;
    context->allocator = 0;
    context->find_memory_index = find_memory_index;

    if (!vulkan_device_create(context, window)) {
        return false;
    }

    if (!vulkan_swapchain_initialize(context, window)) {
        return false;
    }

    return true;
}

void vulkan_renderer_destroy(RendererState state) {
    VulkanContext* context = (VulkanContext*)state.backend_state;

    vulkan_swapchain_destroy(context);
    vulkan_device_destroy(context);

    memory_free(state.backend_state, sizeof(VulkanContext), MEMORY_TAG_VULKAN);
    state.backend_state = 0;
}

bool vulkan_renderer_begin_frame(RendererState* state) { return true; }

void vulkan_renderer_end_frame(RendererState* state) {}

i32 find_memory_index(VulkanContext* context, u32 type_filter,
                      u32 property_flags) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(context->device.gpu, &mem_props);

    for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) && (mem_props.memoryTypes[i].propertyFlags &
                                       property_flags) == property_flags) {
            return i;
        }
    }

    LOG_WARN("Unable to find suitable memory type");
    return -1;
}
