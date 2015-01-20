#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "definitions.h"
#include "config.h"
#include "meminfo.h"
#include "chunk.h"
#include "line.h"
#include "error.h"
#include "debug.h"


/** Configure how many threads to use.
 * Concerned config variable: g_conf.threads
 */
static void     config_threads(void)
{
    long        available_processors;

    available_processors = sysconf(_SC_NPROCESSORS_ONLN);
    if (available_processors < 0)
    {
        error("could not determine available processors");
    }
    else if (available_processors == 0)
    {
        die("unexpected '0' value for sysconf(_SC_NPROCESSORS_ONLN)");
    }
    if (g_conf.threads == 0)
    {
        g_conf.threads = available_processors;
    }
    else if (g_conf.threads > available_processors)
    {
        error("too much threads started (max is %ld)", available_processors);
    }
}


/** Configure max used memory.
 * It checks and sets the following g_conf values:
 *      g_conf.page_size
 *      g_conf.memlimit
 */
static void     config_memlimit(void)
{
    long        max_memory;

    g_conf.page_size = sysconf(_SC_PAGESIZE);
    if (g_conf.page_size < 0)
    {
        error("could not determine page size");
    }
    max_memory = meminfo(MEMAVAILABLE);
    if (max_memory < 0)
    {
        error("meminfo(): Cannot determine available memory");
    }
    if (g_conf.memlimit == 0)
    {
        g_conf.memlimit = max_memory - KEEP_FREE_MEMORY;
    }
    if (g_conf.memlimit < g_conf.page_size)
    {
        error("not enough memory");
    }
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
    int         i;

    n = (n - 1) | 1;
    while (n > 0)
    {
        i = 3;
        while (i && i <= sqrt(n))
        {
            if (n % i == 0)
            {
                i = 0;
            }
            else
            {
                i += 2;
            }
        }
        if (i)
        {
            return (n);
        }
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
    double      hmap_sz;
    double      chunk_sz;

    double      hmap_linecost;
    double      chunk_linecost;
    double      delta;

    hmap_linecost = sizeof(t_line) * (1 / HMAP_LOAD_FACTOR);
    chunk_linecost = MEDIUM_LINE_BYTES;
    delta = chunk_linecost * (double)g_conf.threads;

    hmap_sz = hmap_linecost / (hmap_linecost + delta) * g_conf.memlimit;

    chunk_sz = (g_conf.memlimit - hmap_sz) / g_conf.threads;
    /* chunk_sz = ((int)chunk_sz + page_sz - 1) & ~(page_sz - 1); */
    chunk_sz = (int)(chunk_sz) - ((int)chunk_sz % g_conf.page_size);

    // hmap_sz was bytes, convert to number of cells (t_line structs..)
    g_conf.hmap_size = (size_t)hmap_sz / sizeof(t_line);
    g_conf.hmap_size = get_prev_prime(g_conf.hmap_size);


    g_conf.chunk_size = (size_t) chunk_sz;
    if (g_conf.chunk_size < (size_t)(g_conf.page_size * 3))
    {
        error("missing memory: chunk_size can't be less than 3 pages");
    }
}


static void     dlog_obj_t_conf(t_conf *conf)
{
    DLOG("------------------------------");
    DLOG("conf->memlimit:   %ld", conf->memlimit);
    DLOG("conf->threads:    %d",  conf->threads);
    DLOG("conf->page_size:  %d",  conf->page_size);
    DLOG("conf->hmap_size:  %ld", conf->hmap_size);
    DLOG("conf->chunk_size: %ld", conf->chunk_size);
    DLOG("------------------------------");
}


void            configure(void)
{
    config_threads();
    config_memlimit();
    distribute_memory();

    dlog_obj_t_conf(&g_conf);
}
