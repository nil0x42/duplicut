#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "definitions.h"
#include "exit.h"

#define MEMINFO_FILE    ("/proc/meminfo")
#define BUF_SIZE        (1024)
long        get_available_memory(void)
{
    char    *buf;
    size_t  buf_size;
    FILE    *fp;
    long    value;

    fp = fopen(MEMINFO_FILE, "r");
    if (fp == NULL)
        error("cannot open %s: %s", MEMINFO_FILE, ERRNO);
    buf_size = BUF_SIZE * sizeof(*buf);
    buf = (char*) malloc(buf_size);
    value = -1L;
    while (getline(&buf, &buf_size, fp) >= 0 )
    {
        if (strncmp(buf, "MemAvailable", 12) != 0)
            continue;
        sscanf(buf, "%*s%ld", &value);
        break;
    }
    fclose(fp);
    free((void*)buf);
    if (value == -1L)
        error("cannot get available memory from %s", MEMINFO_FILE);
    value = value - (KEEP_FREE_MEMORY / 1024L);
    return ((size_t)value / 1024L);
}
