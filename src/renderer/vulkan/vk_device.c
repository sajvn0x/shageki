#include "vk_device.h"

#include "app.h"
#include "core/logger.h"
#include "core/memory.h"

bool instance_create(VulkanContext* context);
bool debug_messenger_setup(VulkanContext* context);
bool surface_create(VulkanContext* context, GLFWwindow* window);
bool physical_device_pick(VulkanContext* context);
bool device_create(VulkanContext* context);
bool query_swapchain_suppport(VulkanContext* context);

bool vulkan_device_create(VulkanContext* context, GLFWwindow* window) {
    LOG_TRACE("Vulkan device initializing");

    if (!instance_create(context)) return false;
#if TGT_DEBUG
    if (!debug_messenger_setup(context)) return false;
#endif  // TGT_DEBUG
    if (!surface_create(context, window)) return false;
    if (!physical_device_pick(context)) return false;
    if (!device_create(context)) return false;
    if (!query_swapchain_suppport(context)) return false;

    LOG_TRACE("Vulkan device initialized");
    return true;
}

void vulkan_device_destroy(VulkanContext* context) {
    LOG_TRACE("Vulkan device destructing");

    if (context->device.device)
        vkDestroyDevice(context->device.device, context->allocator);

    if (context->device.surface)
        vkDestroySurfaceKHR(context->device.instance, context->device.surface,
                            context->allocator);

#if TGT_DEBUG
    if (context->device.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                context->device.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug)
            destroy_debug(context->device.instance,
                          context->device.debug_messenger, context->allocator);
    }
#endif  // TGT_DEBUG

    if (context->device.instance)
        vkDestroyInstance(context->device.instance, context->allocator);

    LOG_TRACE("Vulkan device destructed");
}

bool instance_create(VulkanContext* context) {
    const char* INSTANCE_EXTENSIONS[] = {VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(PLATFORM_WINDOWS)
                                         VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_LINUX)
                                         VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
                                         VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    const u32 INSTANCE_EXTENSIONS_COUNT =
        sizeof(INSTANCE_EXTENSIONS) / sizeof(INSTANCE_EXTENSIONS[0]);

    const char* INSTANCE_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
    u32 INSTANCE_LAYERS_COUNT =
        sizeof(INSTANCE_LAYERS) / sizeof(INSTANCE_LAYERS[0]);

    VkApplicationInfo app_info = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                  .pApplicationName = APP_NAME,
                                  .pEngineName = APP_ENGINE,
                                  .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                  .apiVersion = VK_API_VERSION_1_3};

    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = INSTANCE_EXTENSIONS_COUNT,
        .ppEnabledExtensionNames = INSTANCE_EXTENSIONS,
        .enabledLayerCount = INSTANCE_LAYERS_COUNT,
        .ppEnabledLayerNames = INSTANCE_LAYERS};

    if (vkCreateInstance(&ci, context->allocator, &context->device.instance) !=
        VK_SUCCESS) {
        LOG_ERROR("Failed to create vulkan instance")
        return false;
    }

    return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            LOG_TRACE("[VALIDATION] %s", data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_INFO("[VALIDATION] %s", data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARN("[VALIDATION] %s", data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR("[VALIDATION] %s", data->pMessage);
            return VK_TRUE;
        default:
            break;
    }

    return VK_FALSE;
}

bool debug_messenger_setup(VulkanContext* context) {
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
            context->device.instance, "vkCreateDebugUtilsMessengerEXT");

    if (!create_debug) {
        LOG_ERROR(
            "Failed to get function pointer: vkCreateDebugUtilsMessengerEXT");
        return false;
    }

    if (create_debug(context->device.instance, &ci, context->allocator,
                     &context->device.debug_messenger) != VK_SUCCESS) {
        LOG_ERROR("Failed to setup vulkan debugger");
        return false;
    }

    return true;
}

bool surface_create(VulkanContext* context, GLFWwindow* window) {
    if (glfwCreateWindowSurface(context->device.instance, window,
                                context->allocator,
                                &context->device.surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create surface");
        return false;
    }

    return true;
}

bool physical_device_pick(VulkanContext* context) {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(context->device.instance, &device_count, NULL);
    if (device_count == 0) {
        LOG_ERROR("No Vulkan-capable physical devices found");
        return false;
    }

    VkPhysicalDevice* devices = memory_allocate(
        device_count * sizeof(VkPhysicalDevice), MEMORY_TAG_VULKAN);
    if (!devices) {
        LOG_ERROR("Failed to allocate memory for device list");
        return false;
    }
    vkEnumeratePhysicalDevices(context->device.instance, &device_count,
                               devices);

    for (u32 i = 0; i < device_count; ++i) {
        u32 queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                                 NULL);
        VkQueueFamilyProperties* queues = memory_allocate(
            queue_count * sizeof(VkQueueFamilyProperties), MEMORY_TAG_VULKAN);
        if (!queues) {
            LOG_ERROR("Failed to allocate memory for queue families");
            memory_free(devices, device_count * sizeof(VkPhysicalDevice),
                        MEMORY_TAG_VULKAN);
            return false;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                                 queues);

        for (uint32_t j = 0; j < queue_count; ++j) {
            if (!(queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;

            VkBool32 present_support = VK_FALSE;
            VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
                devices[i], j, context->device.surface, &present_support);
            if (res != VK_SUCCESS) {
                LOG_WARN(
                    "vkGetPhysicalDeviceSurfaceSupportKHR failed for device "
                    "%u, family %u (ignoring)",
                    i, j);
                continue;
            }

            if (present_support) {
                context->device.gpu = devices[i];
                context->device.graphics_queue_index = j;
                context->device.present_queue_index = j;

                memory_free(queues,
                            queue_count * sizeof(VkQueueFamilyProperties),
                            MEMORY_TAG_VULKAN);
                memory_free(devices, device_count * sizeof(VkPhysicalDevice),
                            MEMORY_TAG_VULKAN);
                LOG_INFO("Selected physical device %u (queue family %u)", i, j);
                return true;
            }
        }
        memory_free(queues, queue_count * sizeof(VkQueueFamilyProperties),
                    MEMORY_TAG_VULKAN);
    }

    memory_free(devices, device_count * sizeof(VkPhysicalDevice),
                MEMORY_TAG_VULKAN);
    LOG_ERROR("No physical device with graphics + present support found.");
    return false;
}

bool device_create(VulkanContext* context) {
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = context->device.graphics_queue_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    const char* DEVICE_EXTENSIONS[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const u32 DEVICE_EXTENSION_COUNT =
        sizeof(DEVICE_EXTENSIONS) / sizeof(DEVICE_EXTENSIONS[0]);
    VkPhysicalDeviceFeatures features = {0};

    VkDeviceCreateInfo device_ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_ci,
        .enabledExtensionCount = DEVICE_EXTENSION_COUNT,
        .ppEnabledExtensionNames = DEVICE_EXTENSIONS,
        .pEnabledFeatures = &features};

    VkResult res = vkCreateDevice(context->device.gpu, &device_ci,
                                  context->allocator, &context->device.device);
    if (res != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device (VkResult %d)", res);
        return false;
    }

    vkGetDeviceQueue(context->device.device,
                     context->device.graphics_queue_index, 0,
                     &context->device.graphics_queue);
    vkGetDeviceQueue(context->device.device,
                     context->device.present_queue_index, 0,
                     &context->device.present_queue);

    return true;
}

bool query_swapchain_suppport(VulkanContext* context) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        context->device.gpu, context->device.surface,
        &context->device.swapchain_support.capabilities);

    // surface format
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device.gpu, context->device.surface, &format_count, NULL);
    VkSurfaceFormatKHR* formats = memory_allocate(
        format_count * sizeof(VkSurfaceFormatKHR), MEMORY_TAG_VULKAN);
    if (!formats) {
        LOG_ERROR("Failed to allocate memory for VkSurfaceFormatKHR");
        return false;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        context->device.gpu, context->device.surface, &format_count, formats);

    VkSurfaceFormatKHR chosen_format = formats[0];
    for (u32 i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_format = formats[i];
            break;
        }
    }

    context->device.swapchain_support.format = chosen_format.format;
    context->device.swapchain_support.color_space = chosen_format.colorSpace;
    memory_free(formats, format_count * sizeof(VkSurfaceFormatKHR),
                MEMORY_TAG_VULKAN);

    // present mode
    u32 mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device.gpu, context->device.surface, &mode_count, NULL);
    VkPresentModeKHR* modes = memory_allocate(
        mode_count * sizeof(VkPresentModeKHR), MEMORY_TAG_VULKAN);
    if (!modes) {
        LOG_ERROR("Failed to allocate memory for VkPresentModeKHR");
        return false;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        context->device.gpu, context->device.surface, &mode_count, modes);
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < mode_count; i++) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    context->device.swapchain_support.present_mode = present_mode;
    memory_free(modes, format_count * sizeof(VkSurfaceFormatKHR),
                MEMORY_TAG_VULKAN);

    return true;
}
