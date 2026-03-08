#ifndef VULKAN_H
#define VULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_context.h"

#define VK_CHECK(call)                                                         \
    do {                                                                       \
        VkResult result = call;                                                \
        if (result != VK_SUCCESS) {                                            \
            fprintf(stderr, "Vulkan error at %s:%d: %d\n", __FILE__, __LINE__, \
                    result);                                                   \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

static VkShaderModule create_shader_module(VkDevice device,
                                           const char* spv_path);
VkResult create_instance(VulkanContext* ctx);
VkResult setup_debug_messenger(VulkanContext* ctx);
VkResult create_surface(VulkanContext* ctx, GLFWwindow* window);
VkResult pick_physical_device(VulkanContext* ctx, VkSurfaceKHR surface);
VkResult create_logical_device(VulkanContext* ctx);
VkResult create_swapchain(VulkanContext* ctx, GLFWwindow* window);
VkResult create_image_views(VulkanContext* ctx);
VkResult create_render_pass(VulkanContext* ctx);
VkResult create_descriptor_set_layout(VulkanContext* ctx);
VkResult create_graphics_pipeline(VulkanContext* ctx, const char* vert_spv,
                                  const char* frag_spv);

VkResult create_framebuffers(VulkanContext* ctx);
VkResult create_command_pool(VulkanContext* ctx);
VkResult create_vertex_buffer(VulkanContext* ctx, Vertex* vertices,
                              uint32_t vertex_count);
VkResult create_index_buffer(VulkanContext* ctx, uint16_t* indices,
                             uint32_t index_count);

// texture
VkResult create_texture_image(VulkanContext* ctx, const char* filename);
VkResult create_texture_image_view(VulkanContext* ctx);
VkResult create_texture_sampler(VulkanContext* ctx);

VkResult create_depth_resources(VulkanContext* ctx);
VkResult create_uniform_buffer(VulkanContext* ctx);
VkResult create_descriptor_pool(VulkanContext* ctx);
VkResult allocate_descriptor_set(VulkanContext* ctx);
VkResult update_descriptor_set(VulkanContext* ctx);
VkResult create_command_buffers(VulkanContext* ctx);
VkResult create_sync_objects(VulkanContext* ctx);
void cleanup_vulkan(VulkanContext* ctx);

#endif  // VULKAN_H
