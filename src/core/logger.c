#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

void log_output(LogLevel level, const char* message, ...) {
    const char* level_strings[] = {"[FATAL]: ", "[ERROR]: ", "[ WARN]: ",
                                   "[ INFO]: ", "[DEBUG]: ", "[TRACE]: "};

    char buffer[4096];

    va_list args;
    va_start(args, message);
    vsnprintf(buffer, sizeof(buffer), message, args);
    va_end(args);

    fprintf(stderr, "%s%s\n", level_strings[level], buffer);
    fflush(stderr);
}
