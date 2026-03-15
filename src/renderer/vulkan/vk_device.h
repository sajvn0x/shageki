#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

#include "core/defines.h"
#include "vk_types.h"

bool vulkan_device_create(VulkanContext* context, GLFWwindow* window);
void vulkan_device_destroy(VulkanContext* context);

#endif  // VULKAN_DEVICE_H
