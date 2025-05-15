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
#  include "cprintferr.h"

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-function"
static void _dlog(int level, const char *file, int line, const char *fmt, ...)
{
    time_t      now;
    struct tm*  tm_info;
    char        time_str[12];
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, 12, "%d %H:%M:%S", tm_info);

    va_list     ap;
    char        msg[256];
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    cprintferr(
            "\r\e[2K"
            "\e[34;1m["
            "\e[33;1mDLOG%d \e[0;35m%s \e[1;35m%13s:%-3d"
            "\e[34;1m]"
            ":\e[0m %s\n",
            level, time_str, file+4, line, msg
            );
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
