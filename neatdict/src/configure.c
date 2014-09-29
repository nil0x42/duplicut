#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "config.h"
#include "chunk.h"
#include "line.h"
#include "exit.h"
#include "debug.h"


/** Configure tmpdir variable.
 * If undefined by command line arguments, tmpdir
 * defaults to $TMPDIR environment varialbe.
 */
static void config_tmpdir(void)
{
    if (g_conf.tmpdir == NULL)
    {
        g_conf.tmpdir = getenv("TMPDIR");
        if (g_conf.tmpdir == NULL)
            g_conf.tmpdir = DEFAULT_TMPDIR;
    }
    if (strlen(g_conf.tmpdir) + strlen(CHUNK_FILENAME) >= CHUNK_PATHSIZE)
        error("'%s' exceeds tmpdir maximum length.");
}


/** Configure how many threads to use.
 * Concerned config variable: g_conf.threads
 */
static void config_cores(void)
{
    long    available_processors;

    available_processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (available_processors < 0)
        error("could not determine available processors");
    else if (available_processors == 0)
        die("unexpected '0' value for available_processors");
    if (g_conf.threads == 0)
        g_conf.threads = available_processors;
    else if (g_conf.threads > available_processors)
        error("too much threads started (max is %ld)", available_processors);
}


/** Dirty function to get currently available memory bytes
 */
#define MEMINFO_FILE     ("/proc/meminfo")
#define MEMINFO_BUF_SIZE (1024)
static long get_available_memory(void)
{
    char    *buf;
    size_t  buf_size;
    FILE    *fp;
    long    value;

    fp = fopen(MEMINFO_FILE, "r");
    if (fp == NULL)
        error("cannot open %s: %s", MEMINFO_FILE, ERRNO);
    buf_size = MEMINFO_BUF_SIZE * sizeof(*buf);
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
    return (value * 1024);
}


/** Configure max used memory.
 * It checks and sets the following g_conf values:
 *      g_conf.page_size
 *      g_conf.memlimit
 */
static void config_memlimit(void)
{
    long    max_memory;

    g_conf.page_size = sysconf(_SC_PAGESIZE);
    if (g_conf.page_size < 0)
        error("could not determine page size");
    max_memory = get_available_memory();
    if (g_conf.memlimit == 0)
        g_conf.memlimit = max_memory - KEEP_FREE_MEMORY;
    if (g_conf.memlimit < g_conf.page_size)
        error("not enough memory");
    else if (g_conf.memlimit > max_memory)
    {
        max_memory /= (1024 * 1024);
        error("memlimit exceeds available memory (%ldMB).", max_memory);
    }
}


/** Gives the nearest prime number <= `n`.
 * Used in order to settle hmap size with a prime value.
 * It ensures an optimal hash repartition.
 */
static long     get_prev_prime(long n)
{
    int             i;

    n = (n - 1) | 1;
    while (n > 0)
    {
        i = 3;
        while (i && i <= sqrt(n))
        {
            if (n % i == 0)
                i = 0;
            else
                i += 2;
        }
        if (i)
            return (n);
        n -= 2;
    }
    return (n);
}


/** Get an optimal memory repartition
 * between hasmap and chunk size according
 * to available memory.
 */
static void     distribute_memory(void)
{
    double          hmap_sz;
    double          chunk_sz;

    double          hmap_linecost;
    double          chunk_linecost;
    double          delta;

    hmap_linecost = sizeof(t_line) * (1 / HMAP_LOAD_FACTOR);
    chunk_linecost = MEDIUM_LINE_BYTES;
    delta = chunk_linecost * (double)g_conf.threads;

    hmap_sz = hmap_linecost / (hmap_linecost + delta) * g_conf.memlimit;

    chunk_sz = (g_conf.memlimit - hmap_sz) / g_conf.threads;
    /* chunk_sz = ((int)chunk_sz + page_sz - 1) & ~(page_sz - 1); */
    chunk_sz = (int)(chunk_sz) - ((int)chunk_sz % g_conf.page_size);

    // hmap_sz was bytes, convert to number of cells (t_line structs..)
    g_conf.hmap_size = (size_t)hmap_sz / sizeof(t_line);
    /* g_conf.hmap_size = get_prev_prime(g_conf.hmap_size); */


    g_conf.chunk_size = (size_t) chunk_sz;
    if (g_conf.chunk_size < (size_t)(g_conf.page_size * 3))
        error("missing memory: chunk_size can't be less than 3 pages");
}


void        configure(void)
{
    config_tmpdir();
    config_cores();
    config_memlimit();
    distribute_memory();
    DLOG("------------------------------");
    DLOG("g_conf.memlimit:   %ld", g_conf.memlimit);
    DLOG("g_conf.threads:    %d",  g_conf.threads);
    DLOG("g_conf.tmpdir:     %s",  g_conf.tmpdir);
    DLOG("g_conf.page_size:  %d",  g_conf.page_size);
    DLOG("g_conf.hmap_size:  %ld", g_conf.hmap_size);
    DLOG("g_conf.chunk_size: %ld", g_conf.chunk_size);
    DLOG("------------------------------");
}
