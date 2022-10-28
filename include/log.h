#ifndef RTPTUN_LOG_H
#define RTPTUN_LOG_H

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

#define DEFAULT_LOG_LEVEL LOG_INFO

void _log(log_level_t type, int print_errno, const char *file, int line, const char *format, ...);

void log_level_set(log_level_t level);
log_level_t log_level_get();

#define log_info(format, args...) _log(LOG_INFO, 0, __FILE__, __LINE__, format, ##args)
#define log_warn(format, args...) _log(LOG_WARN, 0, __FILE__, __LINE__, format, ##args)
#define log_error(format, args...) _log(LOG_ERROR, 0, __FILE__, __LINE__, format, ##args)

#define elog_info(format, args...) _log(LOG_INFO, 1, __FILE__, __LINE__, format, ##args)
#define elog_warn(format, args...) _log(LOG_WARN, 1, __FILE__, __LINE__, format, ##args)
#define elog_error(format, args...) _log(LOG_ERROR, 1, __FILE__, __LINE__, format, ##args)

#ifdef DEBUG
#define log_debug(format, args...) _log(LOG_DEBUG, 0, __FILE__, __LINE__, format, ##args)
#else
#define log_debug(msg, args...)
#endif

#define die(format, args...) _log(LOG_FATAL, __FILE__, __LINE__, format, ##args)

#endif