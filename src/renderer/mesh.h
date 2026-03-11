#ifndef MESH_H
#define MESH_H

#include "buffer.h"
#include "core/error.h"

typedef struct {
    Buffer* vertex_buffer;
    Buffer* index_buffer;
    uint32_t index_count;
} Mesh;

AppResult mesh_create_cube(Mesh* mesh, VkPhysicalDevice gpu, VkDevice device);
void mesh_destroy(Mesh* mesh, VkDevice device);

#endif  // MESH_H
