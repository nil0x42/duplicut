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
 *      --> g_conf.threads
 */
static void     config_threads(void)
{
    long        max_threads;

    max_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (max_threads < 0)
        error("sysconf(_SC_NPROCESSORS_ONLN): %s", ERRNO);
    else if (max_threads == 0)
        die("sysconf(_SC_NPROCESSORS_ONLN): Unexpected return: 0");

    if (g_conf.threads == 0)
        g_conf.threads = max_threads;
    else if (g_conf.threads > max_threads)
        error("Cannot use more than %ld threads", max_threads);
}


/** Get memory page size.
 *      --> g_conf.page_size
 */
static void     config_page_size(void)
{
    int         page_size;

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0)
        error("sysconf(_SC_PAGESIZE): %s", ERRNO);
    g_conf.page_size = page_size;
}


/** Configure max used memory.
 *      --> g_conf.memlimit
 */
static void     config_memlimit(void)
{
    long        max_memory;

    max_memory = meminfo(MEMAVAILABLE);
    if (max_memory < 0)
        error("meminfo(MEMAVAILABLE): Cannot determine available memory");

    if (g_conf.memlimit == 0)
        g_conf.memlimit = max_memory;

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


/** Return the nearest prime number <= `n`.
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


void            configure(void)
{
    config_threads();
    config_page_size();

    config_memlimit();

    distribute_memory();

    DLOG("--------configure()-----------");
    DLOG("------------------------------");
    DLOG("conf->memlimit:      %ld", g_conf.memlimit);
    DLOG("conf->threads:       %u", g_conf.threads);
    DLOG("conf->line_max_size: %u", g_conf.line_max_size);
    DLOG("conf->page_size:     %d", g_conf.page_size);
    DLOG("conf->hmap_size:     %ld", g_conf.hmap_size);
    DLOG("conf->chunk_size:    %ld", g_conf.chunk_size);
    DLOG("------------------------------");
}
