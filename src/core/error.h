#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

typedef enum AppResult {
    APP_SUCCESS = 0,
    APP_ERROR_VULKAN,
    APP_ERROR_MEMORY,
    APP_ERROR_IO,
    APP_ERROR_INVALID_STATE
} AppResult;

AppResult vk_to_app_result(VkResult r) {
    if (r == VK_SUCCESS) return APP_SUCCESS;
    return APP_ERROR_VULKAN;
}

#define LOG_ERROR(...) fprintf(stderr, "[ERROR] " __VA_ARGS__)

#define LOG_INFO(...) fprintf(stdout, "[INFO] " __VA_ARGS__)

#define CHECK_ALLOC(ptr, msg) \
    if ((ptr)) {              \
        LOG_ERROR(msg "\n");  \
        return NULL;          \
    }

#define VK_CHECK(x)                               \
    do {                                          \
        VkResult err = x;                         \
        if (err != VK_SUCCESS) {                  \
            LOG_ERROR("Vulkan error: %d\n", err); \
            return err;                           \
        }                                         \
    } while (0)

#endif  // ERROR_H
