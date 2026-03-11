#include "mesh.h"

#include "core/error.h"
#include "material.h"
#include "renderer/buffer.h"

static const Vertex cube_vertices[] = {
    // Front face
    {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    // Back face
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
    // Left face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    // Right face
    {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}},
    // Top face
    {{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    // Bottom face
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}}};

static const uint16_t cube_indices[] = {
    0,  1,  2,  2,  3,  0,   // front
    4,  5,  6,  6,  7,  4,   // back
    8,  9,  10, 10, 11, 8,   // left
    12, 13, 14, 14, 15, 12,  // right
    16, 17, 18, 18, 19, 16,  // top
    20, 21, 22, 22, 23, 20   // bottom
};

AppResult mesh_create_cube(Mesh* mesh, VkPhysicalDevice gpu, VkDevice device) {
    // Vertex buffer
    mesh->vertex_buffer =
        buffer_create(gpu, device, sizeof(cube_vertices), 24,
                      (void*)cube_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!mesh->vertex_buffer) return APP_ERROR_VULKAN;

    // Index buffer
    mesh->index_buffer =
        buffer_create(gpu, device, sizeof(cube_indices), 36,
                      (void*)cube_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!mesh->index_buffer) {
        buffer_destroy(mesh->vertex_buffer, device);
        return APP_ERROR_VULKAN;
    }

    mesh->index_count = 36;
    return APP_SUCCESS;
}

void mesh_destroy(Mesh* mesh, VkDevice device) {
    if (mesh->index_buffer) buffer_destroy(mesh->index_buffer, device);
    if (mesh->vertex_buffer) buffer_destroy(mesh->vertex_buffer, device);
}
