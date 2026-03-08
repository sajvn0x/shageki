#include <stddef.h>  // for offsetof
#include <stdint.h>  // for UINT64_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vulkan_context.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/cglm.h>
#define STB_IMAGE_IMPLEMENTATION
#include "core/fs.h"
#include "stb_image/stb_image.h"
#include "vulkan.h"

const char* APP_NAME = "Shageki";
const char* APP_ENGINE = "Shageki";

static VkShaderModule create_shader_module(VkDevice device,
                                           const char* spv_path) {
    size_t code_size;
    char* code = read_file(spv_path, &code_size);

    if (!code) {
        fprintf(stderr, "Failed to read shader %s\n", spv_path);
        exit(1);
    }
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code_size;
    ci.pCode = (uint32_t*)code;
    VkShaderModule module;
    VkResult res = vkCreateShaderModule(device, &ci, NULL, &module);
    free(code);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "Failed to create shader module from %s (error %d)\n",
                spv_path, res);
        exit(1);
    }
    return module;
}

uint32_t find_memory_type(VulkanContext* ctx, uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkResult create_instance(VulkanContext* ctx) {
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = APP_NAME;
    app_info.pEngineName = APP_ENGINE;
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
                                VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
                                VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
                                VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    uint32_t ext_count = sizeof(extensions) / sizeof(extensions[0]);

    const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layer_count = 1;

    VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app_info;
    ci.enabledExtensionCount = ext_count;
    ci.ppEnabledExtensionNames = extensions;
    ci.enabledLayerCount = layer_count;
    ci.ppEnabledLayerNames = layers;

    return vkCreateInstance(&ci, NULL, &ctx->instance);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    fprintf(stderr, "[Validation] %s\n", data->pMessage);
    return VK_FALSE;
}

VkResult setup_debug_messenger(VulkanContext* ctx) {
    VkDebugUtilsMessengerCreateInfoEXT ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = NULL};

    PFN_vkCreateDebugUtilsMessengerEXT create_debug =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            ctx->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!create_debug) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return create_debug(ctx->instance, &ci, NULL, &ctx->debug_messenger);
}

VkResult create_surface(VulkanContext* ctx, GLFWwindow* window) {
    return glfwCreateWindowSurface(ctx->instance, window, NULL, &ctx->surface);
}

VkResult pick_physical_device(VulkanContext* ctx, VkSurfaceKHR surface) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
    if (device_count == 0) return VK_ERROR_DEVICE_LOST;

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; i++) {
        uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                                 NULL);
        VkQueueFamilyProperties* queues =
            malloc(queue_count * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                                 queues);

        for (uint32_t j = 0; j < queue_count; j++) {
            if (queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, surface,
                                                     &present_support);
                if (present_support) {
                    ctx->physical_device = devices[i];
                    ctx->graphics_family = j;
                    ctx->present_family = j;
                    break;
                }
            }
        }
        free(queues);
        if (ctx->physical_device != VK_NULL_HANDLE) break;
    }
    free(devices);

    if (ctx->physical_device == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult create_logical_device(VulkanContext* ctx) {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_ci.queueFamilyIndex = ctx->graphics_family;
    queue_ci.queueCount = 1;
    queue_ci.pQueuePriorities = &queue_priority;

    const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo device_ci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &queue_ci;
    device_ci.enabledExtensionCount = 1;
    device_ci.ppEnabledExtensionNames = device_extensions;
    VkPhysicalDeviceFeatures features = {0};
    device_ci.pEnabledFeatures = &features;

    VkResult res =
        vkCreateDevice(ctx->physical_device, &device_ci, NULL, &ctx->device);
    if (res == VK_SUCCESS) {
        vkGetDeviceQueue(ctx->device, ctx->graphics_family, 0,
                         &ctx->graphics_queue);
        vkGetDeviceQueue(ctx->device, ctx->present_family, 0,
                         &ctx->present_queue);
    }
    return res;
}

VkResult create_swapchain(VulkanContext* ctx, GLFWwindow* window) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->physical_device,
                                              ctx->surface, &caps);

    // Choose surface format
    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface,
                                         &format_count, NULL);
    VkSurfaceFormatKHR* formats =
        malloc(format_count * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->physical_device, ctx->surface,
                                         &format_count, formats);

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = formats[i];
            break;
        }
    }
    ctx->swapchain_format = chosen_format.format;
    ctx->swapchain_color_space = chosen_format.colorSpace;
    free(formats);

    // Present mode
    uint32_t mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device,
                                              ctx->surface, &mode_count, NULL);
    VkPresentModeKHR* modes = malloc(mode_count * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx->physical_device,
                                              ctx->surface, &mode_count, modes);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < mode_count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(modes);

    // Extent
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        ctx->swapchain_extent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        ctx->swapchain_extent.width = w;
        ctx->swapchain_extent.height = h;
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = ctx->surface;
    ci.minImageCount = image_count;
    ci.imageFormat = ctx->swapchain_format;
    ci.imageColorSpace = ctx->swapchain_color_space;
    ci.imageExtent = ctx->swapchain_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = present_mode;
    ci.clipped = VK_TRUE;

    return vkCreateSwapchainKHR(ctx->device, &ci, NULL, &ctx->swapchain);
}

VkResult create_image_views(VulkanContext* ctx) {
    uint32_t count;
    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &count, NULL);
    VkImage* images = malloc(count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(ctx->device, ctx->swapchain, &count, images);

    ctx->swapchain_image_count = count;
    ctx->swapchain_image_views = malloc(count * sizeof(VkImageView));

    for (uint32_t i = 0; i < count; i++) {
        VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ci.image = images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = ctx->swapchain_format;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(ctx->device, &ci, NULL,
                                         &ctx->swapchain_image_views[i]);
        if (res != VK_SUCCESS) {
            free(images);
            return res;
        }
    }
    free(images);
    return VK_SUCCESS;
}

VkResult create_render_pass(VulkanContext* ctx) {
    VkAttachmentDescription attachments[2] = {0};

    // Colour attachment
    attachments[0].format = ctx->swapchain_format;
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
    VkAttachmentReference depth_ref = {
        1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkRenderPassCreateInfo rp_ci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp_ci.attachmentCount = 2;
    rp_ci.pAttachments = attachments;
    rp_ci.subpassCount = 1;
    rp_ci.pSubpasses = &subpass;

    return vkCreateRenderPass(ctx->device, &rp_ci, NULL, &ctx->render_pass);
}

VkCommandBuffer begin_single_time_commands(VulkanContext* ctx) {
    VkCommandBufferAllocateInfo alloc = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = ctx->command_pool;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx->device, &alloc, &cmd);

    VkCommandBufferBeginInfo begin = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void end_single_time_commands(VulkanContext* ctx, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx->graphics_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx->graphics_queue);
    vkFreeCommandBuffers(ctx->device, ctx->command_pool, 1, &cmd);
}

VkResult create_texture_image(VulkanContext* ctx, const char* filename) {
    int tex_width, tex_height, tex_channels;
    stbi_uc* pixels = stbi_load(filename, &tex_width, &tex_height,
                                &tex_channels, STBI_rgb_alpha);

    if (!pixels) {
        fprintf(stderr, "Failed to load texture image %s\n", filename);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkDeviceSize image_size = tex_width * tex_height * 4;

    // create staging buffer
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    VkBufferCreateInfo buffer_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VK_CHECK(vkCreateBuffer(ctx->device, &buffer_ci, NULL, &staging_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(ctx->device, staging_buffer, &mem_reqs);
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex =
            find_memory_type(ctx, mem_reqs.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };
    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &staging_memory));
    VK_CHECK(
        vkBindBufferMemory(ctx->device, staging_buffer, staging_memory, 0));

    void* data;
    vkMapMemory(ctx->device, staging_memory, 0, image_size, 0, &data);
    memcpy(data, pixels, image_size);
    vkUnmapMemory(ctx->device, staging_memory);
    stbi_image_free(pixels);

    VkImageCreateInfo image_ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.extent.width = tex_width;
    image_ci.extent.height = tex_height;
    image_ci.extent.depth = 1;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.format = VK_FORMAT_R8G8B8A8_SRGB;  // typical format
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateImage(ctx->device, &image_ci, NULL, &ctx->texture_image));

    vkGetImageMemoryRequirements(ctx->device, ctx->texture_image, &mem_reqs);
    alloc.allocationSize = mem_reqs.size;
    alloc.memoryTypeIndex = find_memory_type(
        ctx, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &ctx->texture_memory));
    VK_CHECK(vkBindImageMemory(ctx->device, ctx->texture_image,
                               ctx->texture_memory, 0));

    // Transition the image to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    VkCommandBuffer cmd = begin_single_time_commands(ctx);
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = ctx->texture_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &barrier);
    end_single_time_commands(ctx, cmd);

    // Copy staging buffer to image
    VkBufferImageCopy region = {0};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = tex_width;
    region.imageExtent.height = tex_height;
    region.imageExtent.depth = 1;

    cmd = begin_single_time_commands(ctx);
    vkCmdCopyBufferToImage(cmd, staging_buffer, ctx->texture_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    end_single_time_commands(ctx, cmd);

    // Transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL for sampling
    cmd = begin_single_time_commands(ctx);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
    end_single_time_commands(ctx, cmd);

    // Clean up staging resources
    vkDestroyBuffer(ctx->device, staging_buffer, NULL);
    vkFreeMemory(ctx->device, staging_memory, NULL);

    return VK_SUCCESS;
}

VkResult create_texture_image_view(VulkanContext* ctx) {
    VkImageViewCreateInfo view_ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = ctx->texture_image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = VK_FORMAT_R8G8B8A8_SRGB;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    return vkCreateImageView(ctx->device, &view_ci, NULL,
                             &ctx->texture_image_view);
}

VkResult create_texture_sampler(VulkanContext* ctx) {
    VkSamplerCreateInfo sampler_ci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_ci.anisotropyEnable = VK_FALSE;
    sampler_ci.maxAnisotropy = 1.0f;
    sampler_ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_ci.unnormalizedCoordinates = VK_FALSE;
    sampler_ci.compareEnable = VK_FALSE;
    sampler_ci.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.mipLodBias = 0.0f;
    sampler_ci.minLod = 0.0f;
    sampler_ci.maxLod = 0.0f;
    return vkCreateSampler(ctx->device, &sampler_ci, NULL,
                           &ctx->texture_sampler);
}

VkResult create_descriptor_set_layout(VulkanContext* ctx) {
    VkDescriptorSetLayoutBinding bindings[2] = {0};
    // binding 0: Uniform buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 2;
    ci.pBindings = bindings;
    return vkCreateDescriptorSetLayout(ctx->device, &ci, NULL,
                                       &ctx->descriptor_set_layout);
}

VkResult create_graphics_pipeline(VulkanContext* ctx, const char* vert_spv,
                                  const char* frag_spv) {
    VkShaderModule vert_module = create_shader_module(ctx->device, vert_spv);
    VkShaderModule frag_module = create_shader_module(ctx->device, frag_spv);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vert_module, "main", NULL},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, frag_module, "main", NULL}};

    // Vertex input (using Vertex structure)
    VkVertexInputBindingDescription binding_desc = {
        0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr_descs[3] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, tex_coord)}};

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attr_descs;

    VkPipelineInputAssemblyStateCreateInfo input_asm = {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_asm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0,
                           (float)ctx->swapchain_extent.height,
                           (float)ctx->swapchain_extent.width,
                           -(float)ctx->swapchain_extent.height,
                           0.0f,
                           1.0f};
    VkRect2D scissor = {{0, 0}, ctx->swapchain_extent};
    VkPipelineViewportStateCreateInfo viewport_state = {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // test
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend_att = {0};
    blend_att.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend = {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_att;

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineLayoutCreateInfo layout_ci = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_ci.setLayoutCount = 1;
    layout_ci.pSetLayouts = &ctx->descriptor_set_layout;
    VkResult res = vkCreatePipelineLayout(ctx->device, &layout_ci, NULL,
                                          &ctx->pipeline_layout);
    if (res != VK_SUCCESS) return res;

    VkGraphicsPipelineCreateInfo pipeline_ci = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline_ci.stageCount = 2;
    pipeline_ci.pStages = stages;
    pipeline_ci.pVertexInputState = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_asm;
    pipeline_ci.pViewportState = &viewport_state;
    pipeline_ci.pRasterizationState = &rasterizer;
    pipeline_ci.pMultisampleState = &multisample;
    pipeline_ci.pColorBlendState = &blend;
    pipeline_ci.pDepthStencilState = &depth_stencil;
    pipeline_ci.layout = ctx->pipeline_layout;
    pipeline_ci.renderPass = ctx->render_pass;
    pipeline_ci.subpass = 0;

    res =
        vkCreateGraphicsPipelines(ctx->device, VK_NULL_HANDLE, 1, &pipeline_ci,
                                  NULL, &ctx->graphics_pipeline);

    vkDestroyShaderModule(ctx->device, vert_module, NULL);
    vkDestroyShaderModule(ctx->device, frag_module, NULL);
    return res;
}

VkResult create_framebuffers(VulkanContext* ctx) {
    ctx->framebuffers =
        malloc(ctx->swapchain_image_count * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkImageView attachments[2] = {ctx->swapchain_image_views[i],
                                      ctx->depth_image_view};
        VkFramebufferCreateInfo fb_ci = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb_ci.renderPass = ctx->render_pass;
        fb_ci.attachmentCount = 2;
        fb_ci.pAttachments = attachments;
        fb_ci.width = ctx->swapchain_extent.width;
        fb_ci.height = ctx->swapchain_extent.height;
        fb_ci.layers = 1;
        VkResult res = vkCreateFramebuffer(ctx->device, &fb_ci, NULL,
                                           &ctx->framebuffers[i]);
        if (res != VK_SUCCESS) return res;
    }
    return VK_SUCCESS;
}

VkResult create_command_pool(VulkanContext* ctx) {
    VkCommandPoolCreateInfo ci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.queueFamilyIndex = ctx->graphics_family;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(ctx->device, &ci, NULL, &ctx->command_pool);
}

VkResult create_vertex_buffer(VulkanContext* ctx, Vertex* vertices,
                              uint32_t vertex_count) {
    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = sizeof(Vertex) * vertex_count;
    ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(ctx->device, &ci, NULL, &ctx->vertex_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(ctx->device, ctx->vertex_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = mem_reqs.size;

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            mem_type = i;
            break;
        }
    }
    if (mem_type == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    alloc.memoryTypeIndex = mem_type;

    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &ctx->vertex_memory));
    VK_CHECK(vkBindBufferMemory(ctx->device, ctx->vertex_buffer,
                                ctx->vertex_memory, 0));

    void* data;
    vkMapMemory(ctx->device, ctx->vertex_memory, 0, ci.size, 0, &data);
    memcpy(data, vertices, ci.size);
    vkUnmapMemory(ctx->device, ctx->vertex_memory);

    return VK_SUCCESS;
}

VkResult create_index_buffer(VulkanContext* ctx, uint16_t* indices,
                             uint32_t index_count) {
    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = sizeof(uint16_t) * index_count;
    ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(ctx->device, &ci, NULL, &ctx->index_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(ctx->device, ctx->index_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = mem_reqs.size;

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            mem_type = i;
            break;
        }
    }
    if (mem_type == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    alloc.memoryTypeIndex = mem_type;

    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &ctx->index_memory));
    VK_CHECK(vkBindBufferMemory(ctx->device, ctx->index_buffer,
                                ctx->index_memory, 0));

    void* data;
    vkMapMemory(ctx->device, ctx->index_memory, 0, ci.size, 0, &data);
    memcpy(data, indices, ci.size);
    vkUnmapMemory(ctx->device, ctx->index_memory);

    return VK_SUCCESS;
}

VkResult create_depth_resources(VulkanContext* ctx) {
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo image_ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.extent.width = ctx->swapchain_extent.width;
    image_ci.extent.height = ctx->swapchain_extent.height;
    image_ci.extent.depth = 1;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.format = depth_format;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateImage(ctx->device, &image_ci, NULL, &ctx->depth_image));

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(ctx->device, ctx->depth_image, &mem_reqs);
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = mem_reqs.size;

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            mem_type = i;
            break;
        }
    }
    if (mem_type == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    alloc.memoryTypeIndex = mem_type;
    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &ctx->depth_memory));
    VK_CHECK(
        vkBindImageMemory(ctx->device, ctx->depth_image, ctx->depth_memory, 0));

    VkImageViewCreateInfo view_ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = ctx->depth_image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = depth_format;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_ci.subresourceRange.levelCount = 1;
    view_ci.subresourceRange.layerCount = 1;
    VK_CHECK(
        vkCreateImageView(ctx->device, &view_ci, NULL, &ctx->depth_image_view));

    return VK_SUCCESS;
}

VkResult create_uniform_buffer(VulkanContext* ctx) {
    VkBufferCreateInfo ci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = sizeof(UniformBufferObject);
    ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(ctx->device, &ci, NULL, &ctx->uniform_buffer));

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(ctx->device, ctx->uniform_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = mem_reqs.size;

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx->physical_device, &mem_props);
    uint32_t mem_type = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            mem_type = i;
            break;
        }
    }
    if (mem_type == UINT32_MAX) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    alloc.memoryTypeIndex = mem_type;

    VK_CHECK(vkAllocateMemory(ctx->device, &alloc, NULL, &ctx->uniform_memory));
    VK_CHECK(vkBindBufferMemory(ctx->device, ctx->uniform_buffer,
                                ctx->uniform_memory, 0));

    // Map persistently
    vkMapMemory(ctx->device, ctx->uniform_memory, 0,
                sizeof(UniformBufferObject), 0, &ctx->uniform_mapped);

    return VK_SUCCESS;
}

VkResult create_descriptor_pool(VulkanContext* ctx) {
    VkDescriptorPoolSize pool_sizes[2] = {0};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo ci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 1;
    ci.poolSizeCount = 2;
    ci.pPoolSizes = pool_sizes;
    return vkCreateDescriptorPool(ctx->device, &ci, NULL,
                                  &ctx->descriptor_pool);
}

VkResult allocate_descriptor_set(VulkanContext* ctx) {
    VkDescriptorSetAllocateInfo ci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ci.descriptorPool = ctx->descriptor_pool;
    ci.descriptorSetCount = 1;
    ci.pSetLayouts = &ctx->descriptor_set_layout;
    return vkAllocateDescriptorSets(ctx->device, &ci, &ctx->descriptor_set);
}

VkResult update_descriptor_set(VulkanContext* ctx) {
    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = ctx->uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo image_info = {0};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = ctx->texture_image_view;
    image_info.sampler = ctx->texture_sampler;

    VkWriteDescriptorSet writes[2] = {0};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = ctx->descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &buffer_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = ctx->descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &image_info;

    vkUpdateDescriptorSets(ctx->device, 2, writes, 0, NULL);
    return VK_SUCCESS;
}

VkResult create_command_buffers(VulkanContext* ctx) {
    ctx->command_buffers =
        malloc(ctx->swapchain_image_count * sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo ci = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ci.commandPool = ctx->command_pool;
    ci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ci.commandBufferCount = ctx->swapchain_image_count;
    VkResult res =
        vkAllocateCommandBuffers(ctx->device, &ci, ctx->command_buffers);
    if (res != VK_SUCCESS) return res;

    for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
        VkCommandBufferBeginInfo begin = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        vkBeginCommandBuffer(ctx->command_buffers[i], &begin);

        VkClearValue clear_values[2];
        clear_values[0].color = (VkClearColorValue){0.2f, 0.2f, 0.8f, 1.0f};
        clear_values[1].depthStencil = (VkClearDepthStencilValue){1.0f, 0};

        VkRenderPassBeginInfo rp_begin = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rp_begin.renderPass = ctx->render_pass;
        rp_begin.framebuffer = ctx->framebuffers[i];
        rp_begin.renderArea.extent = ctx->swapchain_extent;
        rp_begin.clearValueCount = 2;
        rp_begin.pClearValues = clear_values;

        vkCmdBeginRenderPass(ctx->command_buffers[i], &rp_begin,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(ctx->command_buffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          ctx->graphics_pipeline);
        vkCmdBindDescriptorSets(
            ctx->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
            ctx->pipeline_layout, 0, 1, &ctx->descriptor_set, 0, NULL);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(ctx->command_buffers[i], 0, 1,
                               &ctx->vertex_buffer, &offset);
        vkCmdBindIndexBuffer(ctx->command_buffers[i], ctx->index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(ctx->command_buffers[i], 36, 1, 0, 0, 0);

        vkCmdEndRenderPass(ctx->command_buffers[i]);

        vkEndCommandBuffer(ctx->command_buffers[i]);
    }
    return VK_SUCCESS;
}

VkResult create_sync_objects(VulkanContext* ctx) {
    VkSemaphoreCreateInfo sem_ci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence_ci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult res1 = vkCreateSemaphore(ctx->device, &sem_ci, NULL,
                                      &ctx->image_available_semaphore);
    VkResult res2 = vkCreateSemaphore(ctx->device, &sem_ci, NULL,
                                      &ctx->render_finished_semaphore);
    VkResult res3 =
        vkCreateFence(ctx->device, &fence_ci, NULL, &ctx->in_flight_fence);

    if (res1 != VK_SUCCESS || res2 != VK_SUCCESS || res3 != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

void cleanup_vulkan(VulkanContext* ctx) {
    if (ctx->device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(ctx->device);

    // Destroy sync objects
    if (ctx->render_finished_semaphore)
        vkDestroySemaphore(ctx->device, ctx->render_finished_semaphore, NULL);
    if (ctx->image_available_semaphore)
        vkDestroySemaphore(ctx->device, ctx->image_available_semaphore, NULL);
    if (ctx->in_flight_fence)
        vkDestroyFence(ctx->device, ctx->in_flight_fence, NULL);

    // Vertex buffer
    if (ctx->vertex_buffer)
        vkDestroyBuffer(ctx->device, ctx->vertex_buffer, NULL);
    if (ctx->vertex_memory) vkFreeMemory(ctx->device, ctx->vertex_memory, NULL);

    // Index buffer
    if (ctx->index_buffer)
        vkDestroyBuffer(ctx->device, ctx->index_buffer, NULL);
    if (ctx->index_memory) vkFreeMemory(ctx->device, ctx->index_memory, NULL);

    // Uniform buffer
    if (ctx->uniform_buffer)
        vkDestroyBuffer(ctx->device, ctx->uniform_buffer, NULL);
    if (ctx->uniform_memory)
        vkFreeMemory(ctx->device, ctx->uniform_memory, NULL);

    // Texture
    vkDestroySampler(ctx->device, ctx->texture_sampler, NULL);
    vkDestroyImageView(ctx->device, ctx->texture_image_view, NULL);
    vkDestroyImage(ctx->device, ctx->texture_image, NULL);
    vkFreeMemory(ctx->device, ctx->texture_memory, NULL);

    // Descriptor pool and layout
    if (ctx->descriptor_pool)
        vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
    if (ctx->descriptor_set_layout)
        vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptor_set_layout,
                                     NULL);

    // Depth resources
    if (ctx->depth_image_view)
        vkDestroyImageView(ctx->device, ctx->depth_image_view, NULL);
    if (ctx->depth_image) vkDestroyImage(ctx->device, ctx->depth_image, NULL);
    if (ctx->depth_memory) vkFreeMemory(ctx->device, ctx->depth_memory, NULL);

    // Command buffers and pool
    if (ctx->command_pool) {
        if (ctx->command_buffers) {
            vkFreeCommandBuffers(ctx->device, ctx->command_pool,
                                 ctx->swapchain_image_count,
                                 ctx->command_buffers);
            free(ctx->command_buffers);
        }
        vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
    }

    // Framebuffers
    if (ctx->framebuffers) {
        for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
            if (ctx->framebuffers[i])
                vkDestroyFramebuffer(ctx->device, ctx->framebuffers[i], NULL);
        }
        free(ctx->framebuffers);
    }

    // Pipeline and layout
    if (ctx->graphics_pipeline)
        vkDestroyPipeline(ctx->device, ctx->graphics_pipeline, NULL);
    if (ctx->pipeline_layout)
        vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
    if (ctx->render_pass)
        vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);

    // Image views
    if (ctx->swapchain_image_views) {
        for (uint32_t i = 0; i < ctx->swapchain_image_count; i++) {
            if (ctx->swapchain_image_views[i])
                vkDestroyImageView(ctx->device, ctx->swapchain_image_views[i],
                                   NULL);
        }
        free(ctx->swapchain_image_views);
    }

    // Swapchain
    if (ctx->swapchain)
        vkDestroySwapchainKHR(ctx->device, ctx->swapchain, NULL);

    // Device
    if (ctx->device) vkDestroyDevice(ctx->device, NULL);

    // Surface
    if (ctx->surface && ctx->instance)
        vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);

    // Debug messenger
    if (ctx->debug_messenger && ctx->instance) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebug =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyDebug)
            destroyDebug(ctx->instance, ctx->debug_messenger, NULL);
    }

    // Instance
    if (ctx->instance) vkDestroyInstance(ctx->instance, NULL);

    // Zero out context
    memset(ctx, 0, sizeof(VulkanContext));
}
