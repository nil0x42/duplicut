#ifndef DEBUG_H
# define DEBUG_H

/*
 * DLOG("debug message")
 */
# ifdef DEBUG
#  include <unistd.h>
#  include <stdio.h>
#  include <stdarg.h>
#  include <time.h>

static void _dlog(int level, const char *file, int line, const char *fmt, ...)
{
    char        out[1024] = {0};
    int         i;

    time_t      now;
    struct tm*  tm_info;

    va_list ap;

    if (isatty(STDERR_FILENO))
        i = snprintf(&out[i], 64, "\e[34;1m[\e[33;1mDLOG%d\e[0;35m ", level);
    else
        i = snprintf(&out[i], 64, "[DLOG%d ", level);

    time(&now);
    tm_info = localtime(&now);
    i += strftime(&out[i], 12, "%d %H:%M:%S", tm_info);

    if (isatty(STDERR_FILENO))
        i += snprintf(&out[i], 128, " \e[1;35m%16s:%-3d\e[34;1m]:\e[0m ",
                &file[4], line);
    else
        i += snprintf(&out[i], 128, " %16s:%-3d]: ",
                &file[4], line);

    va_start(ap, fmt);
    i += vsnprintf(&out[i], sizeof(out) - i, fmt, ap);
    va_end(ap);

    out[i++] = '\n';

    write(STDERR_FILENO, out, i);
}

#  define DLOG(...) (_dlog(1, __FILE__, __LINE__, __VA_ARGS__))
# else
#  define DLOG(...)
# endif

#endif
