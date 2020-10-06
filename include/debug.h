#ifndef DEBUG_H
# define DEBUG_H

/*
 * Define DLOG macros
 */
# if DEBUG >= 1
#  include <unistd.h>
#  include <stdio.h>
#  include <stdarg.h>
#  include <time.h>

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
static void _dlog(int level, const char *file, int line, const char *fmt, ...)
{
    char        out[1024] = {0};
    int         i = 0;

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
        i += snprintf(&out[i], 128, " \e[1;35m%10s:%-3d\e[34;1m]:\e[0m ",
                &file[4], line);
    else
        i += snprintf(&out[i], 128, " %10s:%-3d]: ",
                &file[4], line);

    va_start(ap, fmt);
    i += vsnprintf(&out[i], sizeof(out) - i, fmt, ap);
    va_end(ap);

    out[i++] = '\n';

    write(STDERR_FILENO, out, i);
}
#  pragma GCC diagnostic pop

# endif

# if DEBUG >= 1
#  define DLOG1(...) (_dlog(1, __FILE__, __LINE__, __VA_ARGS__))
# else
# define DLOG1(...)
# endif

# if DEBUG >= 2
#  define DLOG2(...) (_dlog(2, __FILE__, __LINE__, __VA_ARGS__))
# else
# define DLOG2(...)
# endif

# if DEBUG >= 3
#  define DLOG3(...) (_dlog(3, __FILE__, __LINE__, __VA_ARGS__))
# else
# define DLOG3(...)
# endif

# if DEBUG >= 4
#  define DLOG4(...) (_dlog(4, __FILE__, __LINE__, __VA_ARGS__))
# else
# define DLOG4(...)
# endif

#endif
