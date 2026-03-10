#ifndef VK_DEFINES_H
#define VK_DEFINES_H

#include <vulkan/vulkan.h>

// clang-format off
#if defined(_WIN32)
#elif defined(__linux__)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#endif
// clang-format on

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// vulkan memory allocator
static const void* ALLOCATOR = NULL;

static const char* INSTANCE_EXTENSIONS[] = {VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
                                            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
                                            VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
                                            VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
static const uint32_t INSTANCE_EXTENSIONS_COUNT =
    sizeof(INSTANCE_EXTENSIONS) / sizeof(INSTANCE_EXTENSIONS[0]);

static const char* INSTANCE_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
static uint32_t INSTANCE_LAYERS_COUNT =
    sizeof(INSTANCE_LAYERS) / sizeof(INSTANCE_LAYERS[0]);

#endif  // VK_DEFINES_H
