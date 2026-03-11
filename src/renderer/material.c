#include "material.h"

#include <string.h>

#include "core/error.h"
#include "core/fs.h"

static VkResult create_descriptor_set_layout(VkDevice device,
                                             VkDescriptorSetLayout* layout);
static VkResult create_descriptor_pool(VkDevice device, VkDescriptorPool* pool);
static VkResult create_pipeline_layout(VkDevice device,
                                       VkDescriptorSetLayout layout,
                                       VkPipelineLayout* pipeline_layout);
static VkResult create_graphics_pipeline(Renderer* renderer, Material* mat,
                                         const char* vert_spv,
                                         const char* frag_spv);
static VkResult update_descriptor_set(VkDevice device, Material* mat);

static VkShaderModule create_shader_module(VkDevice device,
                                           const char* spv_path);

AppResult material_create(Material* mat, Renderer* renderer,
                          const char* vert_spv, const char* frag_spv,
                          VkImageView texture_view, VkSampler sampler) {
    LOG_INFO("MATERIAL: creating\n");

    // validate inputs
    if (!mat || !renderer || !renderer->core || !renderer->core->device) {
        LOG_ERROR("MATERIAL: invalid renderer or device\n");
        return APP_ERROR_INVALID_PARAM;
    }

    VkDevice device = renderer->core->device;
    VkPhysicalDevice gpu = renderer->core->gpu;

    memset(mat, 0, sizeof(Material));

    mat->texture_view = texture_view;
    mat->sampler = sampler;

    VkResult res =
        create_descriptor_set_layout(device, &mat->descriptor_set_layout);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "MATERIAL: failed to create descriptor set layout (VkResult %d)\n",
            res);
        return vk_to_app_result(res);
    }

    res = create_descriptor_pool(device, &mat->descriptor_pool);
    if (res != VK_SUCCESS) {
        LOG_ERROR("MATERIAL: failed to create descriptor pool (VkResult %d)\n",
                  res);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return vk_to_app_result(res);
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mat->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &mat->descriptor_set_layout};
    res = vkAllocateDescriptorSets(device, &alloc_info, &mat->descriptor_set);
    if (res != VK_SUCCESS) {
        LOG_ERROR("MATERIAL: failed to allocate descriptor set (VkResult %d)\n",
                  res);
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return vk_to_app_result(res);
    }

    VkDeviceSize ubo_size = sizeof(UniformBufferObject);
    mat->uniform_buffer = buffer_create(
        gpu, device, ubo_size, 1, NULL, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!mat->uniform_buffer) {
        LOG_ERROR("MATERIAL: failed to create uniform buffer\n");
        vkFreeDescriptorSets(device, mat->descriptor_pool, 1,
                             &mat->descriptor_set);
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return APP_ERROR_VULKAN;  // buffer_create already logged
    }

    res = update_descriptor_set(device, mat);
    if (res != VK_SUCCESS) {
        LOG_ERROR("MATERIAL: failed to update descriptor set (VkResult %d)\n",
                  res);
        buffer_destroy(mat->uniform_buffer, device);
        vkFreeDescriptorSets(device, mat->descriptor_pool, 1,
                             &mat->descriptor_set);
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return vk_to_app_result(res);
    }

    res = create_pipeline_layout(device, mat->descriptor_set_layout,
                                 &mat->pipeline_layout);
    if (res != VK_SUCCESS) {
        LOG_ERROR("MATERIAL: failed to create pipeline layout (VkResult %d)\n",
                  res);
        buffer_destroy(mat->uniform_buffer, device);
        vkFreeDescriptorSets(device, mat->descriptor_pool, 1,
                             &mat->descriptor_set);
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return vk_to_app_result(res);
    }

    res = create_graphics_pipeline(renderer, mat, vert_spv, frag_spv);
    if (res != VK_SUCCESS) {
        LOG_ERROR(
            "MATERIAL: failed to create graphics pipeline (VkResult %d)\n",
            res);
        vkDestroyPipelineLayout(device, mat->pipeline_layout, ALLOCATOR);
        buffer_destroy(mat->uniform_buffer, device);
        vkFreeDescriptorSets(device, mat->descriptor_pool, 1,
                             &mat->descriptor_set);
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        return vk_to_app_result(res);
    }

    LOG_INFO("MATERIAL: created successfully\n");
    return APP_SUCCESS;
}

void material_destroy(Material* mat, VkDevice device) {
    if (!mat) return;
    LOG_INFO("MATERIAL: destroying\n");

    if (mat->pipeline) {
        vkDestroyPipeline(device, mat->pipeline, ALLOCATOR);
        mat->pipeline = VK_NULL_HANDLE;
    }
    if (mat->pipeline_layout) {
        vkDestroyPipelineLayout(device, mat->pipeline_layout, ALLOCATOR);
        mat->pipeline_layout = VK_NULL_HANDLE;
    }
    if (mat->uniform_buffer) {
        buffer_destroy(mat->uniform_buffer, device);
        mat->uniform_buffer = NULL;
    }
    // Descriptor set is automatically freed when pool is destroyed
    if (mat->descriptor_pool) {
        vkDestroyDescriptorPool(device, mat->descriptor_pool, ALLOCATOR);
        mat->descriptor_pool = VK_NULL_HANDLE;
    }
    if (mat->descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(device, mat->descriptor_set_layout,
                                     ALLOCATOR);
        mat->descriptor_set_layout = VK_NULL_HANDLE;
    }
    mat->texture_view = VK_NULL_HANDLE;
    mat->sampler = VK_NULL_HANDLE;

    LOG_INFO("MATERIAL: destroyed\n");
}

static VkResult create_descriptor_set_layout(VkDevice device,
                                             VkDescriptorSetLayout* layout) {
    VkDescriptorSetLayoutBinding bindings[2] = {0};

    // Binding 0: Uniform buffer (dynamic or static? we use static)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Combined image sampler
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings};
    return vkCreateDescriptorSetLayout(device, &ci, ALLOCATOR, layout);
}

static VkResult create_descriptor_pool(VkDevice device,
                                       VkDescriptorPool* pool) {
    VkDescriptorPoolSize pool_sizes[2] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes};
    return vkCreateDescriptorPool(device, &ci, ALLOCATOR, pool);
}

static VkResult create_pipeline_layout(VkDevice device,
                                       VkDescriptorSetLayout layout,
                                       VkPipelineLayout* pipeline_layout) {
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &layout};
    return vkCreatePipelineLayout(device, &ci, ALLOCATOR, pipeline_layout);
}

static VkResult create_graphics_pipeline(Renderer* renderer, Material* mat,
                                         const char* vert_spv,
                                         const char* frag_spv) {
    VkDevice device = renderer->core->device;

    // Load shader modules
    VkShaderModule vert_module = create_shader_module(device, vert_spv);
    VkShaderModule frag_module = create_shader_module(device, frag_spv);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = frag_module,
         .pName = "main"}};

    // Vertex input description (matches Vertex struct)
    VkVertexInputBindingDescription binding_desc = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attr_descs[3] = {
        {.location = 0,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT,
         .offset = offsetof(Vertex, pos)},
        {.location = 1,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT,
         .offset = offsetof(Vertex, color)},
        {.location = 2,
         .binding = 0,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = offsetof(Vertex, tex_coord)}};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = 3,
        .pVertexAttributeDescriptions = attr_descs};

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    // Viewport and scissor (dynamic recommended, but we'll set static for
    // simplicity)
    VkViewport viewport = {.x = 0.0f,
                           .y = 0.0f,
                           .width = (float)renderer->swapchain->extent.width,
                           .height = (float)renderer->swapchain->extent.height,
                           .minDepth = 0.0f,
                           .maxDepth = 1.0f};
    VkRect2D scissor = {.offset = {0, 0},
                        .extent = renderer->swapchain->extent};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor};

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,  // adjust as needed
        .lineWidth = 1.0f};

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    // Depth/stencil (enable depth test if you have a depth attachment)
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS};

    // Color blending
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE};
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment};

    // Finally, the pipeline create info
    VkGraphicsPipelineCreateInfo pipeline_ci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .layout = mat->pipeline_layout,
        .renderPass = renderer->swapchain->render_pass,
        .subpass = 0};

    VkResult res = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipeline_ci, ALLOCATOR, &mat->pipeline);

    vkDestroyShaderModule(device, vert_module, ALLOCATOR);
    vkDestroyShaderModule(device, frag_module, ALLOCATOR);

    return res;
}

static VkResult update_descriptor_set(VkDevice device, Material* mat) {
    VkDescriptorBufferInfo buffer_info = {.buffer = mat->uniform_buffer->buffer,
                                          .offset = 0,
                                          .range = sizeof(UniformBufferObject)};

    VkWriteDescriptorSet writes[2] = {0};

    // Binding 0: uniform buffer
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = mat->descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &buffer_info;

    // Binding 1: combined image sampler (if texture is provided)
    if (mat->texture_view != VK_NULL_HANDLE && mat->sampler != VK_NULL_HANDLE) {
        VkDescriptorImageInfo image_info = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = mat->texture_view,
            .sampler = mat->sampler};
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = mat->descriptor_set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &image_info;
    }

    uint32_t write_count = (mat->texture_view != VK_NULL_HANDLE) ? 2 : 1;
    vkUpdateDescriptorSets(device, write_count, writes, 0, NULL);
    return VK_SUCCESS;
}

VkShaderModule create_shader_module(VkDevice device, const char* spv_path) {
    size_t code_size;
    char* code = read_file(spv_path, &code_size);
    if (!code) {
        fprintf(stderr, "Failed to read shader %s\n", spv_path);
        exit(1);
    }

    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code_size,
        .pCode = (uint32_t*)code

    };
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
