#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include "vk_types.h"

bool vulkan_swapchain_initialize(VulkanContext* context, GLFWwindow* window);
void vulkan_swapchain_destroy(VulkanContext* context);
bool vulkan_swapchain_recreate(VulkanContext* context);

#endif  // VULKAN_SWAPCHAIN_H
