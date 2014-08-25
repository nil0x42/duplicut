#include <stdio.h>
#include <string.h>
#include "definitions.h"
#include "debug.h"

int         remove_duplicates(void *ptr, off_t off)
{
    void    *eof;
    void    *end;
    int     len;
    int     lines = 0;

    eof = ptr + off;
    while (ptr != eof)
    {
        end = memchr(ptr, '\n', off);
        len = end - ptr + 1;
        ptr = end + 1;
        ++lines;
    }
    DLOG("found %d lines\n", lines);

    return (0);
}
