#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <stdint.h>
#include <vulkan/vulkan.h>
// clang-format off
#include <xcb/xcb.h>
#include <vulkan/vulkan_xcb.h>
// clang-format on
#include <cglm/cglm.h>

typedef struct Vertex {
    float pos[3];
    float color[3];
} Vertex;

typedef struct {
    mat4 model;
    mat4 view;
    mat4 proj;
} UniformBufferObject;

typedef struct VulkanContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_family;
    uint32_t present_family;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkColorSpaceKHR swapchain_color_space;
    VkExtent2D swapchain_extent;
    VkImageView* swapchain_image_views;
    uint32_t swapchain_image_count;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer* framebuffers;
    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_memory;
    // depth resources
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_image_view;
    // index buffer
    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    // uniform buffer
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    void* uniform_mapped;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
} VulkanContext;

#endif
