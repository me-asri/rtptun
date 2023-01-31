#define _POSIX_C_SOURCE 200112L

#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __MINGW32__
#define flockfile _lock_file
#define funlockfile _unlock_file

#define strerror_r(errno, buffer, length) strerror_s(buffer, length, errno)
#endif

volatile log_level_t log_level = DEFAULT_LOG_LEVEL;

void _log(log_level_t type, int print_errno, const char *file, int line, const char *format, ...)
{
    static const char *LEVEL_STR[] = {
        [LOG_DEBUG] = "DEBUG",
        [LOG_INFO] = "INFO",
        [LOG_WARN] = "WARN",
        [LOG_ERROR] = "ERROR",
    };

    if (type < log_level)
        return;

    time_t timer = time(NULL);
    struct tm time_info = {0};
    localtime_r(&timer, &time_info);

    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &time_info);

    flockfile(stderr);

    fprintf(stderr, "%s %s %s:%d: ", time_str, LEVEL_STR[type], file, line);

    va_list args;
    va_start(args, format);

    vfprintf(stderr, format, args);

    va_end(args);

    if (print_errno)
    {
        char strerr[BUFSIZ];
        int err = errno;

        errno = 0;
        strerror_r(err, strerr, sizeof(strerr));
        if (errno != 0)
            fputs(" (Unknown error)", stderr);
        else
            fprintf(stderr, " (%s)", strerr);
    }

    fputc('\n', stderr);
    fflush(stderr);

    funlockfile(stderr);
}

void log_level_set(log_level_t level)
{
    log_level = level;
}

log_level_t log_level_get()
{
    return log_level;
}