#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

#include "core/error.h"
#include "renderer.h"
#include "renderer/vk_defines.h"

VkResult command_pool_create(FrameData* frame_data, Core* core);
VkResult command_buffers_create(FrameData* frame_data, Swapchain* swapchain,
                                VkDevice device);
VkResult sync_object_create(FrameData* frame_data, VkDevice device);

AppResult frame_data_create(Renderer* renderer) {
    LOG_INFO("RENDERER_FRAME_DATA: initializing\n");

    if (!renderer->core | !renderer->core->device) {
        LOG_ERROR("RENDERER_FRAME_DATA: core or device not initialized\n");
        return vk_to_app_result(VK_ERROR_INITIALIZATION_FAILED);
    }

    FrameData* frame_data = malloc(sizeof(FrameData));
    if (!frame_data) {
        LOG_ERROR("RENDERER_FRAME_DATA: memory allocation failed\n");
        return APP_ERROR_MEMORY;
    }
    memset(frame_data, 0, sizeof(FrameData));
    frame_data->command_pool = VK_NULL_HANDLE;

    VkResult res = command_pool_create(frame_data, renderer->core);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_FRAME_DATA: command pool creation failed (VkResult %d)\n",
            res);
        free(frame_data);
        return vk_to_app_result(res);
    }

    res = command_buffers_create(frame_data, renderer->swapchain,
                                 renderer->core->device);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_FRAME_DATA: command buffers creation failed (VkResult "
            "%d)\n",
            res);
        vkDestroyCommandPool(renderer->core->device, frame_data->command_pool,
                             ALLOCATOR);
        free(frame_data);
        return vk_to_app_result(res);
    }

    res = sync_object_create(frame_data, renderer->core->device);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_FRAME_DATA: sync objects creation failed (VkResult %d)\n",
            res);
        vkDestroyCommandPool(renderer->core->device, frame_data->command_pool,
                             ALLOCATOR);
        free(frame_data->command_buffers);
        free(frame_data);
        return vk_to_app_result(res);
    }

    assert(frame_data->command_pool);
    assert(frame_data->command_buffers);
    assert(frame_data->image_available_semaphore);
    assert(frame_data->render_finished_semaphore);
    assert(frame_data->in_flight_fence);

    renderer->frame = frame_data;
    LOG_INFO("RENDERER_FRAME_DATA: initialized\n");
    return APP_SUCCESS;
}

void frame_data_destroy(FrameData* frame_data, VkDevice device) {
    if (!frame_data) return;
    LOG_INFO("RENDERER_FRAME_DATA: destructing\n");

    if (frame_data->command_buffers) {
        free(frame_data->command_buffers);
        frame_data->command_buffers = NULL;
    }

    if (frame_data->command_pool) {
        vkDestroyCommandPool(device, frame_data->command_pool, ALLOCATOR);
        frame_data->command_pool = VK_NULL_HANDLE;
    }

    if (frame_data->render_finished_semaphore) {
        vkDestroySemaphore(device, frame_data->render_finished_semaphore,
                           ALLOCATOR);
        frame_data->render_finished_semaphore = VK_NULL_HANDLE;
    }
    if (frame_data->image_available_semaphore) {
        vkDestroySemaphore(device, frame_data->image_available_semaphore,
                           ALLOCATOR);
        frame_data->image_available_semaphore = VK_NULL_HANDLE;
    }
    if (frame_data->in_flight_fence) {
        vkDestroyFence(device, frame_data->in_flight_fence, ALLOCATOR);
        frame_data->in_flight_fence = VK_NULL_HANDLE;
    }

    LOG_INFO("RENDERER_FRAME_DATA: destructed\n");
}

VkResult command_pool_create(FrameData* frame_data, Core* core) {
    VkCommandPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = core->graphics_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
    return vkCreateCommandPool(core->device, &ci, ALLOCATOR,
                               &frame_data->command_pool);
}

VkResult command_buffers_create(FrameData* frame_data, Swapchain* swapchain,
                                VkDevice device) {
    if (!swapchain || !swapchain->swapchain || !frame_data->command_pool) {
        LOG_ERROR("FRAME_DATA_COMMAND_BUFFERS: dependent objects missing\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t image_count = swapchain->image_count;
    if (image_count == 0) {
        LOG_ERROR("FRAME_DATA_COMMAND_BUFFERS: swapchain image count is 0\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    frame_data->command_buffers = malloc(sizeof(VkCommandBuffer) * image_count);
    if (!frame_data->command_buffers) {
        LOG_ERROR("FRAME_DATA_COMMAND_BUFFERS: memory allocation failed\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkCommandBufferAllocateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = frame_data->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = image_count};

    VkResult res =
        vkAllocateCommandBuffers(device, &ci, frame_data->command_buffers);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "FRAME_DATA_COMMAND_BUFFERS: vkAllocateCommandBuffers failed "
            "(VkResult %d)\n",
            res);
        free(frame_data->command_buffers);
        frame_data->command_buffers = NULL;
        return res;
    }

    return VK_SUCCESS;
}

VkResult sync_object_create(FrameData* frame_data, VkDevice device) {
    LOG_INFO("FRAME_DATA_SYNC_OBJECTS: initialization\n");

    VkSemaphoreCreateInfo sem_ci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_ci = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                  .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    VkResult res = vkCreateSemaphore(device, &sem_ci, ALLOCATOR,
                                     &frame_data->image_available_semaphore);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "FRAME_DATA_SYNC_OBJECTS: failed to create image available "
            "semaphore (VkResult %d)\n",
            res);
        return res;
    }

    res = vkCreateSemaphore(device, &sem_ci, ALLOCATOR,
                            &frame_data->render_finished_semaphore);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "FRAME_DATA_SYNC_OBJECTS: failed to create render finished "
            "semaphore (VkResult %d)\n",
            res);
        vkDestroySemaphore(device, frame_data->image_available_semaphore,
                           ALLOCATOR);
        frame_data->image_available_semaphore = VK_NULL_HANDLE;
        return res;
    }

    res = vkCreateFence(device, &fence_ci, ALLOCATOR,
                        &frame_data->in_flight_fence);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "FRAME_DATA_SYNC_OBJECTS: failed to create in‑flight fence "
            "(VkResult %d)\n",
            res);
        vkDestroySemaphore(device, frame_data->image_available_semaphore,
                           ALLOCATOR);
        vkDestroySemaphore(device, frame_data->render_finished_semaphore,
                           ALLOCATOR);
        frame_data->image_available_semaphore = VK_NULL_HANDLE;
        frame_data->render_finished_semaphore = VK_NULL_HANDLE;
        return res;
    }

    LOG_INFO("FRAME_DATA_SYNC_OBJECTS: initialized\n");
    return VK_SUCCESS;
}
