#pragma once

#include <stddef.h>
#include "engine.h"

#define DEFAULT_LINE_MAX_SIZE (1024)

struct line_sample
{
    size_t  bytes;
    size_t  lines;
    size_t  contents;
    double  min_size;
};


struct              conf
{
    const char      *infile_name;
    const char      *outfile_name;
    const char      *dupfile_name;
    unsigned int    threads;
    unsigned int    line_max_size;
    size_t          hmap_size;
    size_t          chunk_size;
    int             filter_printable;
    int             lowercase_wordlist;
    int             uppercase_wordlist;
    long            memlimit;
    size_t          dedupe_memory;
    size_t          partition_count;
    size_t          partition_workers;
    size_t          mmap_window;
    size_t          page_size;
    enum e_engine   engine;
};

/** Global configuration vars.
 * Initialized in config.c
 */
extern struct conf  g_conf;

/* source file: config.c */
void                config(void);
