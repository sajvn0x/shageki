#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_core.h>

#include "app.h"
#include "core/error.h"
#include "renderer.h"
#include "vk_defines.h"

VkResult instance_create(Core* core);
VkResult debug_messenger_setup(Core* core);
VkResult surface_create(Core* core, GLFWwindow* window);
VkResult physical_device(Core* core);
VkResult logical_device_create(Core* core);

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    fprintf(stderr, "[Validation] %s\n", data->pMessage);
    return VK_FALSE;
}

Core* core_init(GLFWwindow* window) {
    CHECK_ALLOC(window, "Window is not initialized");
    LOG_INFO("RENDERER_CORE: initializing\n");

    Core* core = malloc(sizeof(Core));
    CHECK_ALLOC(core, "Failed to allocate Core structure");
    core = memset(core, 0, sizeof(Core));
    core->instance = VK_NULL_HANDLE;
    core->gpu = VK_NULL_HANDLE;
    core->device = VK_NULL_HANDLE;
    core->surface = VK_NULL_HANDLE;
    core->graphics_family = -1;
    core->present_family = -1;

    VK_CHECK(instance_create(core));
    VK_CHECK(debug_messenger_setup(core));
    VK_CHECK(surface_create(core, window));
    VK_CHECK(physical_device(core));
    VK_CHECK(logical_device_create(core));

    assert(core->instance);
    assert(core->gpu);
    assert(core->device);
    assert(core->surface);
    assert(core->graphics_family >= 0);
    assert(core->present_family >= 0);
    assert(core->graphics_queue);
    assert(core->present_queue);

    LOG_INFO("RENDERER_CORE: initialized\n");
    return core;
}

void core_destroy(Core* core) {
    LOG_INFO("RENDERER_CORE: destructing\n");

    // Device
    if (core->device) vkDestroyDevice(core->device, NULL);

    // Surface
    if (core->surface && core->instance)
        vkDestroySurfaceKHR(core->instance, core->surface, ALLOCATOR);

    // Debug messenger
    if (core->debug_messenger && core->instance) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                core->instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug)
            destroy_debug(core->instance, core->debug_messenger, ALLOCATOR);
    }

    // Instance
    if (core->instance) vkDestroyInstance(core->instance, ALLOCATOR);

    LOG_INFO("RENDERER_CORE: destructed\n");
}

VkResult instance_create(Core* core) {
    LOG_INFO("INSTANCE: initializing\n");

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

    VkResult instance_result =
        vkCreateInstance(&ci, ALLOCATOR, &core->instance);

    LOG_INFO("INSTANCE: initialized\n");
    return instance_result;
}

VkResult debug_messenger_setup(Core* core) {
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
            core->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!create_debug) return VK_ERROR_EXTENSION_NOT_PRESENT;

    return create_debug(core->instance, &ci, ALLOCATOR, &core->debug_messenger);
}

VkResult surface_create(Core* core, GLFWwindow* window) {
    VkResult surface_result = glfwCreateWindowSurface(
        core->instance, window, ALLOCATOR, &core->surface);

    LOG_INFO("SURFACE: initialized\n");
    return surface_result;
}

VkResult physical_device(Core* core) {
    LOG_INFO("PHYSICAL_DEVICE: picking\n");

    if (!core->instance || !core->surface) {
        LOG_ERROR(
            "PHYSICAL_DEVICE: dependent objects are not properly initialized");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(core->instance, &device_count, NULL);
    if (device_count == 0) {
        LOG_ERROR("PHYSICAL_DEVICE: device not found\n");
        return VK_ERROR_DEVICE_LOST;
    }

    VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(core->instance, &device_count, devices);

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
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    devices[i], j, core->surface, &present_support);
                if (present_support) {
                    core->gpu = devices[i];
                    core->graphics_family = j;
                    core->present_family = j;
                    break;
                }
            }
        }
        free(queues);
        if (core->gpu != VK_NULL_HANDLE) break;
    }
    free(devices);

    if (core->gpu == VK_NULL_HANDLE) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    LOG_INFO("PHYSICAL_DEVICE: picked\n");
    return VK_SUCCESS;
}

VkResult logical_device_create(Core* core) {
    LOG_INFO("LOGICAL_DEVICE: creating\n");

    if (!core->instance || !core->gpu) {
        LOG_ERROR(
            "LOGICAL_DEVICE: dependent objects are not properly initialized");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_ci = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_ci.queueFamilyIndex = core->graphics_family;
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

    VkResult res = vkCreateDevice(core->gpu, &device_ci, NULL, &core->device);
    if (res == VK_SUCCESS) {
        vkGetDeviceQueue(core->device, core->graphics_family, 0,
                         &core->graphics_queue);
        vkGetDeviceQueue(core->device, core->present_family, 0,
                         &core->present_queue);
    }

    LOG_INFO("LOGICAL_DEVICE: created\n");
    return res;
}
