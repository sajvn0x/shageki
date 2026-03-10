#ifndef RENDERER_BUFFER_H
#define RENDERER_BUFFER_H

#include <vulkan/vulkan_core.h>

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkBufferUsageFlags usage;
    uint32_t count;
    uint32_t size;
} Buffer;

Buffer* buffer_create(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      uint32_t count, void* data, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags mem_props);

void buffer_destroy(Buffer* buf, VkDevice device);

#endif  // RENDERER_BUFFER_H
