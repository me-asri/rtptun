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

void log_init(log_level_t level);

void _log(log_level_t type, int print_errno, const char *file, int line, const char *format, ...);

log_level_t log_get_level();

#define log_i(format, args...) _log(LOG_INFO, 0, __FILE__, __LINE__, format, ##args)
#define elog_i(format, args...) _log(LOG_INFO, 1, __FILE__, __LINE__, format, ##args)

#define log_w(format, args...) _log(LOG_WARN, 0, __FILE__, __LINE__, format, ##args)
#define elog_w(format, args...) _log(LOG_WARN, 1, __FILE__, __LINE__, format, ##args)

#define log_e(format, args...) _log(LOG_ERROR, 0, __FILE__, __LINE__, format, ##args)
#define elog_e(format, args...) _log(LOG_ERROR, 1, __FILE__, __LINE__, format, ##args)

#define log_d(format, args...) _log(LOG_DEBUG, 0, __FILE__, __LINE__, format, ##args)
#define elog_d(format, args...) _log(LOG_DEBUG, 1, __FILE__, __LINE__, format, ##args)

#define log_f(format, args...) _log(LOG_FATAL, 0, __FILE__, __LINE__, format, ##args)
#define elog_f(format, args...) _log(LOG_FATAL, 1, __FILE__, __LINE__, format, ##args)

#endif