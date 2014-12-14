#include <stdlib.h>
#include <stdio.h>
#include "meminfo.h"
#include "error.h"
#include "bytesize.h"


#define MEMINFO_FILE    ("/proc/meminfo")
#define BUF_SIZE        (1024)


static long get_value(char *ptr, const char *str, size_t str_len)
{
    char    *endptr;
    long    result;

    if (strncmp(ptr, str, str_len) == 0)
    {
        ptr += str_len;
        if (*ptr == ':')
        {
            ptr += 1;
            return (bytesize(ptr));
        }
    }
    return (-1L);
}


/** Get identifier value from /proc/meminfo
 *
 * The meminfo() function returns the value in bytes of
 * an identifier from /proc/meminfo file.
 *
 * If identifier could not be found, the function returns -1.
 */
long        meminfo(const char *identifier)
{
    char    *buf;
    size_t  size;
    FILE    *fp;
    long    result;
    size_t  identifier_len;

    fp = fopen(MEMINFO_FILE, "r");
    if (fp == NULL)
    {
        error("cannot open %s: %s", MEMINFO_FILE, ERRNO);
    }
    size = BUF_SIZE * sizeof(*buf);
    buf = (char*) malloc(size);
    if (buf == NULL)
    {
        die("malloc()");
    }

    result = -1L;
    identifier_len = strlen(identifier);
    while (getline(&buf, &size, fp) >= 0)
    {
        result = get_value(buf, identifier, identifier_len);
        if (result >= 0)
        {
            break;
        }
    }
    fclose(fp);
    free((void*)buf);
    return (result);
}
