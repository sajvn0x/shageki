#ifndef CORE_MEMORY_H
#define CORE_MEMORY_H

#include "defines.h"

typedef enum {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_LINEAR_ALLOCATOR,
    MEMORY_TAG_POOL_ALLOCATOR,
    MEMORY_TAG_DARRAY,
    MEMORY_TAG_DICT,
    MEMORY_TAG_RING_QUEUE,
    MEMORY_TAG_BST,
    MEMORY_TAG_STRING,
    MEMORY_TAG_ENGINE,
    MEMORY_TAG_JOB,
    MEMORY_TAG_TEXTURE,
    MEMORY_TAG_MATERIAL_INSTANCE,
    MEMORY_TAG_RENDERER,
    MEMORY_TAG_GAME,
    MEMORY_TAG_TRANSFORM,
    MEMORY_TAG_BINARY_STRING_TABLE,
    MEMORY_TAG_BINARY_DATA,
    MEMORY_TAG_SCENE,
    MEMORY_TAG_PACKAGE,
    MEMORY_TAG_VULKAN,
    // "External" vulkan allocations, for reporting purposes only.
    MEMORY_TAG_VULKAN_EXT,
    MEMORY_TAG_DIRECT3D,
    MEMORY_TAG_OPENGL,
    // Representation of GPU-local/vram
    MEMORY_TAG_GPU_LOCAL,
    MEMORY_TAG_BITMAP_FONT,
    MEMORY_TAG_SYSTEM_FONT,
    MEMORY_TAG_KEYMAP,
    MEMORY_TAG_HASHTABLE,
    MEMORY_TAG_UI,
    MEMORY_TAG_AUDIO,
    MEMORY_TAG_REGISTRY,
    MEMORY_TAG_PLUGIN,
    MEMORY_TAG_PLATFORM,
    MEMORY_TAG_SERIALIZER,
    MEMORY_TAG_ASSET,
    MEMORY_TAG_EDITOR,

    MEMORY_TAG_MAX_TAGS
} MemoryTag;

void* memory_allocate(u64 size, MemoryTag tag);
void memory_free(void* block, u64 size, MemoryTag tag);
void* memory_zero(void* block, u64 size);
void memory_copy(void* dest, const void* source, u64 size);
void* memory_set(void* dest, i32 value, u64 size);
char* memory_usage_str();

#endif  // CORE_MEMORY_H
