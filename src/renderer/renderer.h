#ifndef RENDERER_H
#define RENDERER_H

#include <GLFW/glfw3.h>

#include "core/defines.h"

typedef struct {
    void* backend_state;
} RendererState;

typedef enum {
    RENDERER_BACKEND_VULKAN,
    RENDERER_BACKEND_OPENGL,
    RENDERER_BACKEND_DIRECTX,
} RendererBackend;

typedef struct {
    bool (*renderer_init)(RendererState* state, GLFWwindow* window);
    void (*renderer_destroy)(RendererState state);
    bool (*renderer_begin_frame)(RendererState* state);
    void (*renderer_end_frame)(RendererState* state);
} Renderer;

#endif  // RENDERER_H
