#include <stddef.h>  // for offsetof
#include <stdint.h>  // for UINT64_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/cglm.h>

#include "vulkan.h"

const char* APP_NAME = "Shageki";
const char* APP_ENGINE = "Shageki";

// Helper: read file into memory
char* read_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* data = malloc(*size);
    fread(data, 1, *size, f);
    fclose(f);
    return data;
}

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
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = NULL};

    PFN_vkCreateDebugUtilsMessengerEXT createDebug =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            ctx->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!createDebug) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return createDebug(ctx->instance, &ci, NULL, &ctx->debug_messenger);
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

VkResult create_descriptor_set_layout(VulkanContext* ctx) {
    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo ci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 1;
    ci.pBindings = &binding;
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
    VkVertexInputAttributeDescription attr_descs[2] = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding_desc;
    vertex_input.vertexAttributeDescriptionCount = 2;
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
    VkDescriptorPoolSize pool_size = {0};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo ci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.maxSets = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes = &pool_size;
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

    VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = ctx->descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(ctx->device, 1, &write, 0, NULL);
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
