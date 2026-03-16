#ifndef RENDERER_VULKAN_H
#define RENDERER_VULKAN_H

#include <vulkan/vulkan_core.h>

#include "core/defines.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// clang-format off
#if defined(PLATFORM_WINDOWS)
#elif defined(PLATFORM_LINUX)
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
#else
#endif
// clang-format on

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkPresentModeKHR present_mode;
    // u32 format_count;
    // VkSurfaceFormatKHR* formats;
    // u32 present_mode_count;
    // VkPresentModeKHR* present_modes;
} VulkanSwapchainSupportInfo;

typedef struct {
    VkInstance instance;
#if TGT_DEBUG
    VkDebugUtilsMessengerEXT debug_messenger;
#endif  // TGT_DEBUG
    VkPhysicalDevice gpu;
    VkDevice device;
    VkSurfaceKHR surface;
    VulkanSwapchainSupportInfo swapchain_support;

    i32 graphics_queue_index;
    i32 present_queue_index;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkRenderPass render_pass;

    VkCommandPool graphics_command_pool;

    // synchrnonization objects
    VkSemaphore image_available_semaphore, render_finished_semaphore;
    VkFence in_flight_fence;

    VkFormat depth_format;
} VulkanDevice;

typedef struct {
    VkSwapchainKHR handle;
    u32 image_count;
    VkExtent2D extent;
    VkImage* images;
    VkImageView* image_views;
    VkFramebuffer* framebuffers;
} VulkanSwapchain;

typedef struct VulkanContext {
    VulkanDevice device;
    VkAllocationCallbacks* allocator;
    VulkanSwapchain swapchain;
    i32 (*find_memory_index)(struct VulkanContext* context, u32 type_filter,
                             u32 property_flags);
} VulkanContext;

#endif  // RENDERER_VULKAN_H
