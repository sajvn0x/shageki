#include "vk_swapchain.h"

#include "core/logger.h"

bool swapchain_create(VulkanContext* context, GLFWwindow* window);

bool vulkan_swapchain_initialize(VulkanContext* context, GLFWwindow* window) {
    if (!swapchain_create(context, window)) {
        return false;
    }

    LOG_TRACE("vulkan swapchain initialized")
    return true;
}

void vulkan_swapchain_destroy(VulkanContext* context) {
    LOG_TRACE("vulkan swapchain destructing")

    if (context->swapchain.handle)
        vkDestroySwapchainKHR(context->device.device, context->swapchain.handle,
                              context->allocator);

    LOG_TRACE("vulkan swapchain destructed")
}

bool vulkan_swapchain_recreate(VulkanContext* context) { return true; }

bool swapchain_create(VulkanContext* context, GLFWwindow* window) {
    VkSurfaceCapabilitiesKHR caps =
        context->device.swapchain_support.capabilities;
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        context->swapchain.extent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        context->swapchain.extent.width = w;
        context->swapchain.extent.height = h;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->device.surface,
        .minImageCount = image_count,
        .imageFormat = context->device.swapchain_support.format,
        .imageColorSpace = context->device.swapchain_support.color_space,
        .imageExtent = context->swapchain.extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = context->device.swapchain_support.present_mode,
        .clipped = VK_TRUE,
    };

    VkResult result =
        vkCreateSwapchainKHR(context->device.device, &ci, context->allocator,
                             &context->swapchain.handle);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain");
        return false;
    }

    return true;
}
