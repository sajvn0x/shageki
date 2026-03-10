#ifndef RENDERER_H
#define RENDERER_H

#include "core/error.h"
#include "vk_defines.h"

typedef struct {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice gpu;
    VkPhysicalDeviceMemoryProperties gpu_mem_props;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_family;
    uint32_t present_family;
} Core;

typedef struct {
    VkDevice device;

    VkSwapchainKHR swapchain;
    VkFormat format;
    VkExtent2D extent;
    VkColorSpaceKHR color_space;
    uint32_t image_count;
    VkImage* images;
    VkImageView* image_views;
    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;
    // VkImage depth_image;
    // VkDeviceMemory depth_memory;
    // VkImageView depth_image_view;
} Swapchain;

typedef struct {
    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;
} FrameData;

typedef struct {
    Core* core;
    Swapchain* swapchain;
    FrameData* frame;
} Renderer;

// core
Core* core_init(GLFWwindow* window);
void core_destroy(Core* core);

// swapchain
AppResult swapchain_init(Renderer* renderer, GLFWwindow* window);
void swapchain_destroy(Swapchain* swapchain);

// frame data
AppResult frame_data_create();
void frame_data_destroy();

// renderer
Renderer* renderer_init(GLFWwindow* window);
void renderer_destroy(Renderer* renderer);

#endif  // RENDERER_H
