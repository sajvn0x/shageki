#ifndef CORE_LOGGER_H
#define CORE_LOGGER_H

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1

#if TGT_RELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#else
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1
#endif

typedef enum {
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
} LogLevel;

void log_output(LogLevel level, const char* message, ...);

#define LOG_FATAL(message, ...) \
    log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__);

#ifndef LOG_ERROR
#define LOG_ERROR(message, ...) \
    log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);
#endif

#if LOG_WARN_ENABLED == 1
#define LOG_WARN(message, ...) \
    log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
#define LOG_WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define LOG_INFO(message, ...) \
    log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define LOG_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define LOG_DEBUG(message, ...) \
    log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
#define LOG_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define LOG_TRACE(message, ...) \
    log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
#define LOG_TRACE(message, ...)
#endif

#endif  // CORE_LOGGER_H
