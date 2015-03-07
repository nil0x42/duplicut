#pragma once

#include <stddef.h>

#define DEFAULT_LINE_MAX_SIZE (24)


struct              conf
{
    const char      *infile_name;
    const char      *outfile_name;
    unsigned int    threads;
    unsigned int    line_max_size;
    size_t          hmap_size;
    size_t          chunk_size;
};

/** Global configuration vars.
 * Actually initialized on main.c
 */
extern struct conf  g_conf;

/* source file: config.c */
void                config(void);
