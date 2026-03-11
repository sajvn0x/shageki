#include "renderer/renderer.h"
#include "app.h"

int main(void) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, APP_NAME, NULL, NULL);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
	}

    glfwDestroyWindow(window);
    glfwTerminate();

	return 0;
}

