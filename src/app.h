#ifndef APP_H
#define APP_H

#include <GLFW/glfw3.h>

#include "core/defines.h"
#include "renderer/renderer.h"

static const char* APP_NAME = "Shageki";
static const char* APP_ENGINE = "Shageki Engine";
static const int WIDTH = 800;
static const int HEIGHT = 600;

typedef enum {
    APP_STATE_UNINITIALIZED,
    APP_STATE_INITIALIZING,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_SHUTDOWN
} AppState;

typedef struct {
    GLFWwindow* window;
    Renderer renderer;
	RendererState renderer_state;

    // app state
    AppState state;
    bool resize_occurred;

    // timing
    f64 last_time;
    f64 delta_time;
    u32 frame_count;
} App;

bool app_initialize(App* out_app);
void app_shutdown(App app);
void app_run_loop(App* app);

#endif  // APP_H
