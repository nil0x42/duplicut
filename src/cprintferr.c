#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <fcntl.h>

static int  g_color_ok = -1;
static bool g_stderr_writable;


/** Check stderr before input/output files can reuse a closed descriptor. */
void init_cprintferr(void)
{
    int flags;

    if (g_color_ok != -1)
        return;
    flags = fcntl(STDERR_FILENO, F_GETFL);
    g_stderr_writable = flags >= 0 && (flags & O_ACCMODE) != O_RDONLY;
    g_color_ok = g_stderr_writable && isatty(STDERR_FILENO);
}

static int strip_ansi_inplace(char *s)
{
    char *src = s, *dst = s;

    while (*src) {
        if (*src == '\x1b' && src[1] == '[') {
            src += 2;
            while (*src && !(*src >= '@' && *src <= '~'))
                ++src;
            if (*src) ++src;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return (int)(dst - s);
}

/* color‑aware stderr printf */
int cprintferr(const char *fmt, ...)
{
    if (g_color_ok == -1)
        init_cprintferr();
    if (!g_stderr_writable)
        return (0);

    char buf[PATH_MAX + 256];
    char *bufptr = buf;

    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    if (len < 1)
        return len;
    else if (len >= (int)sizeof(buf) -1)
        len = sizeof(buf) - 2;

    if (!g_color_ok) {
        // strip leading '\r'
        while (*bufptr == '\r')
            ++bufptr;
        len = strip_ansi_inplace(bufptr);
        if (len > 0 && bufptr[len-1] != '\n') {
            bufptr[len++] = '\n';
            bufptr[len] = '\0';
        }
    }
    if (len > 0) {
        fwrite(bufptr, 1, len, stderr);
        if (bufptr[len-1] != '\n')
            fflush(stderr);
    }
    return len;
}
