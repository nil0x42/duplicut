#ifndef DEBUG_H
# define DEBUG_H

/*
 * DLOG("debug message")
 */
# ifdef DEBUG
#  include <unistd.h>
#  include <stdio.h>
#  include <stdarg.h>

static void dlog_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    dprintf(STDERR_FILENO, "DLOG(%s:%d): ", __FILE__, __LINE__);
    vdprintf(STDERR_FILENO, fmt, ap);
    write(STDERR_FILENO, "\n", 1);
    va_end(ap);
}

#  define DLOG(...) (dlog_printf(__VA_ARGS__))
# else
#  define DLOG(...)
# endif

#endif
