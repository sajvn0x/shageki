#include "vk_swapchain.h"

#include "core/logger.h"
#include "core/memory.h"

bool swapchain_create(VulkanContext* context, GLFWwindow* window);
bool swapchain_image_views(VulkanContext* context);

bool vulkan_swapchain_initialize(VulkanContext* context, GLFWwindow* window) {
    if (!swapchain_create(context, window)) return false;
    if (!swapchain_image_views(context)) return false;

    LOG_TRACE("vulkan swapchain initialized")
    return true;
}

void vulkan_swapchain_destroy(VulkanContext* context) {
    LOG_TRACE("vulkan swapchain destructing");

    if (context->swapchain.image_views) {
        for (u32 i = 0; i < context->swapchain.image_count; ++i) {
            if (context->swapchain.image_views[i])
                vkDestroyImageView(context->device.device,
                                   context->swapchain.image_views[i],
                                   context->allocator);
        }
        memory_free(context->swapchain.image_views,
                    sizeof(VkImageView) * context->swapchain.image_count,
                    MEMORY_TAG_VULKAN);
    }

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

bool swapchain_image_views(VulkanContext* context) {
    vkGetSwapchainImagesKHR(context->device.device, context->swapchain.handle,
                            &context->swapchain.image_count, NULL);
    context->swapchain.images = memory_allocate(
        sizeof(VkImage) * context->swapchain.image_count, MEMORY_TAG_VULKAN);
    vkGetSwapchainImagesKHR(context->device.device, context->swapchain.handle,
                            &context->swapchain.image_count,
                            context->swapchain.images);
    if (!context->swapchain.images) {
        LOG_ERROR("Failed to allocate memory for Swapchain Images");
        return false;
    }

    context->swapchain.image_views =
        memory_allocate(sizeof(VkImageView) * context->swapchain.image_count,
                        MEMORY_TAG_VULKAN);
    if (!context->swapchain.image_views) {
        LOG_ERROR("Failed to allocate memory for Swapchain ImageViews");
        return false;
    }

    for (u32 i = 0; i < context->swapchain.image_count; ++i) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = context->swapchain.images[i],
            .format = context->device.swapchain_support.format,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.levelCount = 1,
            .subresourceRange.layerCount = 1,
        };
        VkResult result =
            vkCreateImageView(context->device.device, &ci, context->allocator,
                              &context->swapchain.image_views[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create Swapchain ImageView");
            return false;
        }
    }

    LOG_INFO("Swapchain Image and Image Views are created");
    return true;
}
