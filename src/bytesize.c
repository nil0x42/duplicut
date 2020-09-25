#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


#define BUF_SIZE 64
#define BYTESIZE_METRICS "BKMGT"


/** Get byte size representation value from string.
 *
 * If `str` doesn't point to a valid byte size representation,
 * then bytesize() function returns -1.
 *
 * Example:
 *      long long size = bytesize("32 KB");
 */
long long   bytesize(const char *str)
{
    long long   result;
    char        *endptr;
    int         c;

    result = strtoll(str, &endptr, 10);
    if (result < 0 || endptr == str)
        return (-1L);

    while (*endptr == ' ' || *endptr == '\t')
        endptr++;

    c = toupper(*endptr);
    if (c == '\0' || c == 'O')
        c = 'B';

    if ((str = strchr(BYTESIZE_METRICS, c)) == NULL)
        return (-1L);

    c = str - BYTESIZE_METRICS;
    while (c--)
        result *= 1024;

    return (result);
}

char    *sizerepr(size_t size)
{
    static char buf[BUF_SIZE] = {0};
    size_t div = 0;
    size_t rem = 0;

    while (size >= 1024 && div < sizeof(BYTESIZE_METRICS)) {
        rem = size % 1024;
        size /= 1024;
        ++div;
    }
    snprintf(buf, BUF_SIZE, "%.1f%c", (double)size + (double)rem / 1024.0, BYTESIZE_METRICS[div]);
    return buf;
}
