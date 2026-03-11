#include <vulkan/vulkan_core.h>

#include "core/error.h"
#include "renderer.h"

VkResult swapchain_create(Core* core, Swapchain* swapchain, GLFWwindow* window);
VkResult render_pass_init(Swapchain* swapchain);
VkResult swapchain_image_views_init(Swapchain* swapchain);
VkResult swapchain_framebuffers_init(Swapchain* swapchain);

AppResult swapchain_init(Renderer* renderer, GLFWwindow* window) {
    LOG_INFO("RENDERER_SWAPCHAIN: initializing\n");

    if (!renderer->core) {
        LOG_ERROR("RENDERER_SWAPCHAIN: core not initialized\n");
        return vk_to_app_result(VK_ERROR_INITIALIZATION_FAILED);
    }

    Core* core = renderer->core;
    if (!core->gpu || !core->device || !core->surface) {
        LOG_ERROR("RENDERER_SWAPCHAIN: core missing required Vulkan objects\n");
        return vk_to_app_result(VK_ERROR_INITIALIZATION_FAILED);
    }

    Swapchain* swapchain = malloc(sizeof(Swapchain));
    if (!swapchain) {
        LOG_ERROR("RENDERER_SWAPCHAIN: memory allocation failed\n");
        return APP_ERROR_MEMORY;
    }
    swapchain->device = core->device;

    VkResult res = swapchain_create(core, swapchain, window);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_SWAPCHAIN: swapchain creation failed (VkResult %d)\n",
            res);
        free(swapchain);
        return vk_to_app_result(res);
    }

    res = render_pass_init(swapchain);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_SWAPCHAIN: render pass creation failed (VkResult %d)\n",
            res);
        vkDestroySwapchainKHR(core->device, swapchain->swapchain, ALLOCATOR);
        free(swapchain);
        return vk_to_app_result(res);
    }

    res = swapchain_image_views_init(swapchain);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_SWAPCHAIN: image views creation failed (VkResult %d)\n",
            res);
        vkDestroyRenderPass(core->device, swapchain->render_pass, ALLOCATOR);
        vkDestroySwapchainKHR(core->device, swapchain->swapchain, ALLOCATOR);
        free(swapchain);
        return vk_to_app_result(res);
    }

    res = swapchain_framebuffers_init(swapchain);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "RENDERER_SWAPCHAIN: framebuffers creation failed (VkResult %d)\n",
            res);
        for (uint32_t i = 0; i < swapchain->image_count; ++i) {
            vkDestroyImageView(core->device, swapchain->image_views[i],
                               ALLOCATOR);
        }
        free(swapchain->image_views);
        vkDestroyRenderPass(core->device, swapchain->render_pass, ALLOCATOR);
        vkDestroySwapchainKHR(core->device, swapchain->swapchain, ALLOCATOR);
        free(swapchain);
        return vk_to_app_result(res);
    }

    renderer->swapchain = swapchain;
    LOG_INFO("RENDERER_SWAPCHAIN: initialized\n");
    return APP_SUCCESS;
}

void swapchain_destroy(Swapchain* swapchain) {
    if (!swapchain) return;
    LOG_INFO("RENDERER_SWAPCHAIN: destructing\n");

    if (swapchain->framebuffers) {
        for (uint32_t i = 0; i < swapchain->image_count; ++i) {
            if (swapchain->framebuffers[i])
                vkDestroyFramebuffer(swapchain->device,
                                     swapchain->framebuffers[i], ALLOCATOR);
        }
        free(swapchain->framebuffers);
        swapchain->framebuffers = NULL;
    }

    if (swapchain->image_views) {
        for (uint32_t i = 0; i < swapchain->image_count; ++i) {
            if (swapchain->image_views[i])
                vkDestroyImageView(swapchain->device, swapchain->image_views[i],
                                   ALLOCATOR);
        }
        free(swapchain->image_views);
        swapchain->image_views = NULL;
    }

    if (swapchain->render_pass) {
        vkDestroyRenderPass(swapchain->device, swapchain->render_pass,
                            ALLOCATOR);
        swapchain->render_pass = VK_NULL_HANDLE;
    }

    if (swapchain->swapchain) {
        vkDestroySwapchainKHR(swapchain->device, swapchain->swapchain,
                              ALLOCATOR);
        swapchain->swapchain = VK_NULL_HANDLE;
    }

    LOG_INFO("RENDERER_SWAPCHAIN: destructed\n");
}

VkResult swapchain_create(Core* core, Swapchain* swapchain,
                          GLFWwindow* window) {
    LOG_INFO("SWAPCHAIN: initializing\n");

    if (!window || !core->gpu || !core->surface) {
        LOG_ERROR(
            "SWAPCHAIN: dependent objects are not properly "
            "initialized\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core->gpu, core->surface, &caps);

    // Choose surface format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(core->gpu, core->surface,
                                         &format_count, NULL);
    VkSurfaceFormatKHR* formats =
        malloc(format_count * sizeof(VkSurfaceFormatKHR));
    if (!formats) {
        LOG_ERROR("SWAPCHAIN: out of memory\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(core->gpu, core->surface,
                                         &format_count, formats);

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = formats[i];
            break;
        }
    }
    swapchain->format = chosen_format.format;
    swapchain->color_space = chosen_format.colorSpace;
    free(formats);

    // Choose present mode (prefer MAILBOX, fallback to FIFO)
    uint32_t mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(core->gpu, core->surface,
                                              &mode_count, NULL);
    VkPresentModeKHR* modes = malloc(mode_count * sizeof(VkPresentModeKHR));
    if (!modes) {
        LOG_ERROR("SWAPCHAIN: out of memory\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(core->gpu, core->surface,
                                              &mode_count, modes);

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < mode_count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(modes);

    // Determine swap extent
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        swapchain->extent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        swapchain->extent.width = w;
        swapchain->extent.height = h;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = core->surface,
        .minImageCount = image_count,
        .imageFormat = swapchain->format,
        .imageColorSpace = swapchain->color_space,
        .imageExtent = swapchain->extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
    };

    VkResult res = vkCreateSwapchainKHR(core->device, &ci, ALLOCATOR,
                                        &swapchain->swapchain);
    if (res != VK_SUCCESS) {
        LOG_ERROR("SWAPCHAIN: vkCreateSwapchainKHR failed (VkResult %d)\n",
                  res);
    } else {
        LOG_INFO("SWAPCHAIN: Vulkan swapchain created\n");
    }
    return res;
}

VkResult render_pass_init(Swapchain* swapchain) {
    LOG_INFO("RENDER_PASS: initializing\n");

    VkAttachmentDescription attachments[2] = {0};

    // Color attachment
    attachments[0].format = swapchain->format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass};

    VkResult res = vkCreateRenderPass(swapchain->device, &ci, ALLOCATOR,
                                      &swapchain->render_pass);
    if (res == VK_SUCCESS)
        LOG_INFO("RENDER_PASS: initialized\n");
    else
        LOG_ERROR("RENDER_PASS: creation failed (VkResult %d)\n", res);
    return res;
}

VkResult swapchain_image_views_init(Swapchain* swapchain) {
    LOG_INFO("SWAPCHAIN: creating image views\n");

    uint32_t image_count;
    vkGetSwapchainImagesKHR(swapchain->device, swapchain->swapchain,
                            &image_count, NULL);
    VkImage* images = malloc(image_count * sizeof(VkImage));
    if (!images) {
        LOG_ERROR("SWAPCHAIN: out of memory for images\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkGetSwapchainImagesKHR(swapchain->device, swapchain->swapchain,
                            &image_count, images);

    swapchain->image_count = image_count;
    swapchain->image_views = malloc(sizeof(VkImageView) * image_count);
    if (!swapchain->image_views) {
        LOG_ERROR("SWAPCHAIN: out of memory for image views\n");
        free(images);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .format = swapchain->format,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.levelCount = 1,
            .subresourceRange.layerCount = 1,
        };
        VkResult res = vkCreateImageView(swapchain->device, &ci, ALLOCATOR,
                                         &swapchain->image_views[i]);
        if (res != VK_SUCCESS) {
            LOG_ERROR(
                "SWAPCHAIN: failed to create image view %u (VkResult %d)\n", i,
                res);
            // Clean up already created views
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(swapchain->device, swapchain->image_views[j],
                                   ALLOCATOR);
            }
            free(swapchain->image_views);
            swapchain->image_views = NULL;
            free(images);
            return res;
        }
    }

    free(images);
    LOG_INFO("SWAPCHAIN: image views created\n");
    return VK_SUCCESS;
}

VkResult swapchain_framebuffers_init(Swapchain* swapchain) {
    LOG_INFO("SWAPCHAIN: creating framebuffers\n");

    if (!swapchain || !swapchain->render_pass || !swapchain->image_views) {
        LOG_ERROR(
            "SWAPCHAIN_FRAMEBUFFERS: missing render pass or image views\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    swapchain->framebuffers =
        malloc(sizeof(VkFramebuffer) * swapchain->image_count);
    if (!swapchain->framebuffers) {
        LOG_ERROR("SWAPCHAIN_FRAMEBUFFERS: out of memory\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (uint32_t i = 0; i < swapchain->image_count; ++i) {
        VkImageView attachments[1] = {swapchain->image_views[i]};
        VkFramebufferCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = swapchain->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = swapchain->extent.width,
            .height = swapchain->extent.height,
            .layers = 1,
        };
        VkResult res = vkCreateFramebuffer(swapchain->device, &ci, ALLOCATOR,
                                           &swapchain->framebuffers[i]);
        if (res != VK_SUCCESS) {
            LOG_ERROR(
                "SWAPCHAIN_FRAMEBUFFERS: failed to create framebuffer %u "
                "(VkResult %d)\n",
                i, res);
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(swapchain->device,
                                     swapchain->framebuffers[j], ALLOCATOR);
            }
            free(swapchain->framebuffers);
            swapchain->framebuffers = NULL;
            return res;
        }
    }

    LOG_INFO("SWAPCHAIN: framebuffers created\n");
    return VK_SUCCESS;
}
