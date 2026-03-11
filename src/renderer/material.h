#ifndef MATERIAL_H
#define MATERIAL_H

#include <cglm/types.h>

#include "buffer.h"
#include "core/error.h"
#include "renderer.h"

typedef struct Vertex {
    vec3 pos;
    vec3 color;
    vec2 tex_coord;
} Vertex;

typedef struct {
    mat4 model;
    mat4 view;
    mat4 proj;
} UniformBufferObject;

typedef struct {
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkImageView texture_view;
    VkSampler sampler;
    Buffer* uniform_buffer;
} Material;

/**
 * Create a material.
 * @param mat               Pointer to uninitialized Material (memory allocated
 * by caller)
 * @param renderer          The renderer context
 * @param vert_spv          Path to vertex shader SPIR-V
 * @param frag_spv          Path to fragment shader SPIR-V
 * @param texture_view      Already created image view for the texture (or
 * VK_NULL_HANDLE if no texture)
 * @param sampler           Already created sampler for the texture (or
 * VK_NULL_HANDLE)
 * @return APP_SUCCESS on success, otherwise error code.
 */
AppResult material_create(Material* mat, Renderer* renderer,
                          const char* vert_spv, const char* frag_spv,
                          VkImageView texture_view, VkSampler sampler);

/**
 * Destroy a material and free all associated Vulkan objects.
 */
void material_destroy(Material* mat, VkDevice device);

#endif  // MATERIAL_H
