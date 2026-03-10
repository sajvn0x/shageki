#ifndef APP_H
#define APP_H

#include <stdbool.h>

static const char* APP_NAME = "Shageki";
static const char* APP_ENGINE = "Shageki Engine";
static const int WIDTH = 800;
static const int HEIGHT = 600;

typedef struct {
    int width, height;
    bool is_initializing, is_initialized, is_paused;
} App;

#endif // APP_H
