#include <stddef.h>  // for offsetof
#include <stdint.h>  // for UINT64_MAX
#include <string.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/cglm.h>

#include "vulkan.h"

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Shageki", NULL, NULL);

    VulkanContext ctx = {0};

    // Cube vertices (8 vertices, 3D positions + colors)
    Vertex cube_vertices[] = {{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                              {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                              {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
                              {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
                              {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},
                              {{0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
                              {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}}};

    uint16_t cube_indices[] = {
        0, 1, 2, 2, 3, 0,  // front
        1, 5, 6, 6, 2, 1,  // right
        5, 4, 7, 7, 6, 5,  // back
        4, 0, 3, 3, 7, 4,  // left
        3, 2, 6, 6, 7, 3,  // top
        4, 5, 1, 1, 0, 4   // bottom
    };

    VK_CHECK(create_instance(&ctx));
    VK_CHECK(setup_debug_messenger(&ctx));
    VK_CHECK(create_surface(&ctx, window));
    VK_CHECK(pick_physical_device(&ctx, ctx.surface));
    VK_CHECK(create_logical_device(&ctx));
    VK_CHECK(create_swapchain(&ctx, window));
    VK_CHECK(create_image_views(&ctx));
    VK_CHECK(create_depth_resources(&ctx));
    VK_CHECK(create_render_pass(&ctx));

    // Descriptor set layout must be created before pipeline
    VK_CHECK(create_descriptor_set_layout(&ctx));

    VK_CHECK(
        create_graphics_pipeline(&ctx, "shader.vert.spv", "shader.frag.spv"));
    VK_CHECK(create_framebuffers(&ctx));
    VK_CHECK(create_command_pool(&ctx));

    // Create buffers
    VK_CHECK(create_vertex_buffer(&ctx, cube_vertices, 8));
    VK_CHECK(create_index_buffer(&ctx, cube_indices, 36));
    VK_CHECK(create_uniform_buffer(&ctx));

    // Descriptor pool and set
    VK_CHECK(create_descriptor_pool(&ctx));
    VK_CHECK(allocate_descriptor_set(&ctx));
    VK_CHECK(update_descriptor_set(&ctx));

    VK_CHECK(create_command_buffers(&ctx));
    VK_CHECK(create_sync_objects(&ctx));

    float angle = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(ctx.device, 1, &ctx.in_flight_fence, VK_TRUE,
                        UINT64_MAX);
        vkResetFences(ctx.device, 1, &ctx.in_flight_fence);

        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(
            ctx.device, ctx.swapchain, UINT64_MAX,
            ctx.image_available_semaphore, VK_NULL_HANDLE, &image_index);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            break;
        }

        // Update uniform buffer
        angle += 0.0001f;
        UniformBufferObject ubo = {0};
        glm_mat4_identity(ubo.model);
        glm_rotate_y(ubo.model, angle, ubo.model);

        vec3 eye = {2.0f, 2.0f, 2.0f};
        vec3 center = {0.0f, 0.0f, 0.0f};
        vec3 up = {0.0f, 1.0f, 0.0f};
        glm_lookat(eye, center, up, ubo.view);

        glm_perspective(
            glm_rad(45.0f),
            (float)ctx.swapchain_extent.width / ctx.swapchain_extent.height,
            0.1f, 10.0f, ubo.proj);
        // Vulkan clip space has Y inverted
        ubo.proj[1][1] *= -1;

        memcpy(ctx.uniform_mapped, &ubo, sizeof(ubo));

        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &ctx.image_available_semaphore;
        VkPipelineStageFlags wait_stages =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit.pWaitDstStageMask = &wait_stages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &ctx.command_buffers[image_index];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &ctx.render_finished_semaphore;

        vkQueueSubmit(ctx.graphics_queue, 1, &submit, ctx.in_flight_fence);

        VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &ctx.render_finished_semaphore;
        present.swapchainCount = 1;
        present.pSwapchains = &ctx.swapchain;
        present.pImageIndices = &image_index;

        vkQueuePresentKHR(ctx.present_queue, &present);
    }

    vkDeviceWaitIdle(ctx.device);
    cleanup_vulkan(&ctx);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
