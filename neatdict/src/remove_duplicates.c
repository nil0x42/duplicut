#include <stdio.h>
#include <string.h>
#include "definitions.h"
#include "debug.h"

#include <unistd.h>

int         remove_duplicates(char *ptr, off_t off)
{
    char    *eof;
    char    *end;
    int     len;
    int     lines = 0;

    eof = ptr + off;
    while (ptr != eof)
    {
        end = memchr(ptr, '\n', off);
        len = end - ptr;
        write(1, ptr, len + 1);
        ptr = end + 1;
        ++lines;
    }
    printf("\n\n\n\n\n\n\n\n\n");
    DLOG("");
    DLOG("found %d lines\n", lines);
    return (0);
}
