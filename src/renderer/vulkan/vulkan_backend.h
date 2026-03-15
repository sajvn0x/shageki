#include "core/defines.h"
#include "renderer/renderer.h"

bool vulkan_renderer_init(RendererState* state, GLFWwindow* window);
void vulkan_renderer_destroy(RendererState state);
bool vulkan_renderer_begin_frame(RendererState* state);
void vulkan_renderer_end_frame(RendererState* state);
