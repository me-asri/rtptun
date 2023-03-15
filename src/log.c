#define _POSIX_C_SOURCE 200112L

#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>

#define ANSI_RESET "\033[0m"
#define ANSI_FG(color, str) "\033[0;" color "m" str ANSI_RESET

#define ANSI_FG_RED(str) ANSI_FG("31", str)
#define ANSI_FG_GREEN(str) ANSI_FG("32", str)
#define ANSI_FG_YELLOW(str) ANSI_FG("33", str)
#define ANSI_FG_CYAN(str) ANSI_FG("36", str)
#define ANSI_FG_BRIGHT_CYAN(str) ANSI_FG("96", str)
#define ANSI_FG_GREY(str) ANSI_FG("90", str)

static log_level_t log_level = DEFAULT_LOG_LEVEL;
static bool log_color = false;

void log_init(log_level_t level)
{
    log_level = level;
    log_color = (isatty(fileno(stderr)) == 1);
}

void _log(log_level_t type, int print_errno, const char *file, int line, const char *format, ...)
{
    static const char *LEVEL_STR[] = {
        [LOG_DEBUG] = "DEBUG",
        [LOG_INFO] = "INFO",
        [LOG_WARN] = "WARN",
        [LOG_ERROR] = "ERROR",
        [LOG_FATAL] = "FATAL",
    };
    static const char *LEVEL_STR_COLOR[] = {
        [LOG_DEBUG] = ANSI_FG_GREY("DEBUG"),
        [LOG_INFO] = ANSI_FG_GREEN("INFO"),
        [LOG_WARN] = ANSI_FG_YELLOW("WARN"),
        [LOG_ERROR] = ANSI_FG_RED("ERROR"),
        [LOG_FATAL] = ANSI_FG_RED("FATAL"),
    };

    if (type < log_level)
        return;

    time_t timer = time(NULL);
    struct tm time_info = {0};
    localtime_r(&timer, &time_info);

    char time_str[9];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &time_info);

    flockfile(stderr);

    if (log_color)
        fprintf(stderr, ANSI_FG_GREY("%s") " %s " ANSI_FG_CYAN("%s") ":" ANSI_FG_BRIGHT_CYAN("%d") " ",
                time_str, LEVEL_STR_COLOR[type], file, line);
    else
        fprintf(stderr, "%s %s %s:%d ", time_str, LEVEL_STR[type], file, line);

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

    if (type == LOG_FATAL)
        exit(EXIT_FAILURE);
}

log_level_t log_get_level()
{
    return log_level;
}