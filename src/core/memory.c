#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"

typedef struct {
    u64 total_allocated;
    u64 target_allocations[MEMORY_TAG_MAX_TAGS];
} MemoryStats;

static MemoryStats memory_stats;

static const char* memory_tag_strings[MEMORY_TAG_MAX_TAGS] = {
    "UNKNOWN    ", "ARRAY      ", "LINEAR_ALLC", "POOL_ALLOC ", "DARRAY     ",
    "DICT       ", "RING_QUEUE ", "BST        ", "STRING     ", "ENGINE     ",
    "JOB        ", "TEXTURE    ", "MAT_INST   ", "RENDERER   ", "GAME       ",
    "TRANSFORM  ", "BIN_STR_TBL", "BINARY_DATA", "SCENE      ", "RESOURCE   ",
    "VULKAN     ", "VULKAN_EXT ", "DIRECT3D   ", "OPENGL     ", "GPU_LOCAL  ",
    "BITMAP_FONT", "SYSTEM_FONT", "KEYMAP     ", "HASHTABLE  ", "UI         ",
    "AUDIO      ", "REGISTRY   ", "PLUGIN     ", "PLATFORM   ", "SERIALIZER ",
    "ASSET      ", "EDITOR     "};

void* memory_allocate(u64 size, MemoryTag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        LOG_WARN("memory_allocate called using MEMORY_TAG_UNKNOWN");
    }

    memory_stats.total_allocated += size;
    memory_stats.target_allocations[tag] += size;

    void* block = malloc(size);
    block = memory_zero(block, size);

    return block;
}

void memory_free(void* block, u64 size, MemoryTag tag) {
    if (tag == MEMORY_TAG_UNKNOWN) {
        LOG_WARN("memory_free called using MEMORY_TAG_UNKNOWN");
    }

    memory_stats.total_allocated -= size;
    memory_stats.target_allocations[tag] -= size;

    free(block);
}

void* memory_zero(void* block, u64 size) { return memset(block, 0, size); }

void memory_copy(void* dest, const void* source, u64 size) {
    memcpy(dest, source, size);
}

void* memory_set(void* dest, i32 value, u64 size) {
    return memset(dest, value, size);
}

char* memory_usage_str() {
    const u64 gib = 1024 * 1024 * 1024;
    const u64 mib = 1024 * 1024;
    const u64 kib = 1024;

    char buffer[8000] = "System memory use (tagged):\n";
    u64 offset = strlen(buffer);
    for (u32 i = 0; i < MEMORY_TAG_MAX_TAGS; ++i) {
        char unit[4] = "XiB";
        float amount = 1.0f;

        if (memory_stats.target_allocations[i] >= gib) {
            unit[0] = 'G';
            amount = memory_stats.target_allocations[i] / (float)gib;
        } else if (memory_stats.target_allocations[i] >= mib) {
            unit[0] = 'M';
            amount = memory_stats.target_allocations[i] / (float)mib;
        } else if (memory_stats.target_allocations[i] >= kib) {
            unit[0] = 'K';
            amount = memory_stats.target_allocations[i] / (float)kib;
        } else {
            unit[0] = 'B';
            unit[1] = 0;
            amount = memory_stats.target_allocations[i];
        };

        i32 length = snprintf(buffer + offset, 8000, "\t%s: %.2f%s\n",
                              memory_tag_strings[i], amount, unit);
        offset += length;
    }
    char* out_string = strdup(buffer);
    return out_string;
}
