#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

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
    static int color_ok = -1;
    if (color_ok == -1)
        color_ok = isatty(STDERR_FILENO);

    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    if (len < 1)
        return len;
    else if (len >= (int)sizeof(buf) -1)
        len = sizeof(buf) - 2;

    if (!color_ok) {
        len = strip_ansi_inplace(buf);
        if (len > 0 && buf[len-1] != '\n') {
            buf[len++] = '\n';
            buf[len] = '\0';
        }
    }
    if (len > 0) {
        fwrite(buf, 1, len, stderr);
        if (buf[len-1] != '\n')
            fflush(stderr);
    }
    return len;
}
