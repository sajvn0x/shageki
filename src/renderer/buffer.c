#include "buffer.h"

#include <string.h>

#include "core/error.h"
#include "renderer/vk_defines.h"

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties);

Buffer* buffer_create(VkPhysicalDevice gpu, VkDevice device, VkDeviceSize size,
                      uint32_t count, void* data, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags mem_props) {
    LOG_INFO("BUFFER: creation\n");

    if (size == 0) {
        LOG_ERROR("BUFFER: Buffer size cannot be zero\n");
        return NULL;
    }

    Buffer* buf = malloc(sizeof(Buffer));
    if (!buf) {
        LOG_ERROR("BUFFER: Failed to allocate Buffer\n");
        return NULL;
    }
    buf->size = (uint32_t)size;
    buf->count = count;
    buf->usage = usage;

    // Buffer creation
    VkBufferCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = size,
                             .usage = usage,
                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    VkResult res = vkCreateBuffer(device, &ci, ALLOCATOR, &buf->buffer);
    if (res != VK_SUCCESS) {
        LOG_ERROR("BUFFER: Failed to create buffer (error %d)\n", res);
        free(buf);
        return NULL;
    }

    // Memory requirements
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, buf->buffer, &mem_reqs);

    uint32_t mem_type =
        find_memory_type(gpu, mem_reqs.memoryTypeBits, mem_props);
    if (mem_type == UINT32_MAX) {
        LOG_ERROR("BUFFER: No suitable memory type found\n");
        free(buf);
        return NULL;
    }

    // Allocate memory
    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = mem_type};
    res = vkAllocateMemory(device, &alloc, ALLOCATOR, &buf->memory);
    if (res != VK_SUCCESS) {
        LOG_ERROR("BUFFER: Failed to allocate memory (error %d)", res);
        vkDestroyBuffer(device, buf->buffer, ALLOCATOR);
        free(buf);
        return NULL;
    }

    // Bind memory
    res = vkBindBufferMemory(device, buf->buffer, buf->memory, 0);
    if (res != VK_SUCCESS) {
        LOG_ERROR("BUFFER: Failed to bind buffer memory (error %d)\n", res);
        vkFreeMemory(device, buf->memory, ALLOCATOR);
        vkDestroyBuffer(device, buf->buffer, ALLOCATOR);
        free(buf);
        return NULL;
    }

    // Copy data if provided
    if (data != NULL) {
        void* mapped;
        res = vkMapMemory(device, buf->memory, 0, size, 0, &mapped);
        if (res != VK_SUCCESS) {
            LOG_ERROR("BUFFER: Failed to map memory (error %d)\n", res);
            vkFreeMemory(device, buf->memory, ALLOCATOR);
            vkDestroyBuffer(device, buf->buffer, ALLOCATOR);
            free(buf);
            return NULL;
        }
        memcpy(mapped, data, (size_t)size);
        vkUnmapMemory(device, buf->memory);
    }

    LOG_INFO("BUFFER: created\n");

    return buf;
}

void buffer_destroy(Buffer* buf, VkDevice device) {
    LOG_INFO("BUFFER: destructing\n");

    if (buf->buffer) vkDestroyBuffer(device, buf->buffer, ALLOCATOR);
    if (buf->memory) vkFreeMemory(device, buf->memory, ALLOCATOR);

    LOG_INFO("BUFFER: destructed\n");
}

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }
    return UINT32_MAX;
}
