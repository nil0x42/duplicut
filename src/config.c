#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "const.h"
#include "config.h"
#include "memstate.h"
#include "chunk.h"
#include "line.h"
#include "file.h"
#include "error.h"
#include "debug.h"
#include "bytesize.h"
#include "partition.h"

#ifndef PARTITION_MIN_CHUNKS
# define PARTITION_MIN_CHUNKS 16
#endif


/** Configuration --> g_conf.threads
 * Determine the number of worker threads to use.
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
        g_conf.threads = (unsigned int) max_threads;
    else if (g_conf.threads > (unsigned int) max_threads)
        error("cannot use more than %ld threads", max_threads);
}


/** Get biggest prime number `p` as `p` <= `n`.
 * Used to choose a prime number of hash table slots.
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


/** Configuration --> g_conf.hmap_size
 * Estimate `hmap` capacity from file size and the memory budget.
 */
static void     config_hmap_size(struct file *file, struct memstate *memstate)
{
    double      max_size;
    double      hmap_size;

    hmap_size = (double)file->info.st_size;
    hmap_size /= HMAP_LOAD_FACTOR * AVG_LINE_BYTES;
    if (hmap_size < 10000.0)
        hmap_size = 10000.0;

    max_size = memstate->mem_budget * HMAP_MAX_SIZE;
    max_size /= sizeof(t_line);
    if (hmap_size > max_size)
        hmap_size = max_size;

    g_conf.hmap_size = (size_t) get_prev_prime((long)hmap_size);
}


/** Configuration --> g_conf.chunk_size
 * Estimate a target chunk size from the hash table capacity.
 * get_next_chunk() extends chunk boundaries to the next newline.
 */
static void     config_chunk_size(struct file *file)
{
    double      file_size;
    double      portions;
    double      chunk_size;

    file_size = (double) file->info.st_size;
    chunk_size = (double) g_conf.hmap_size;
    chunk_size *= HMAP_LOAD_FACTOR * AVG_LINE_BYTES;
    portions = round(file_size / chunk_size);
    if (portions < 1.0)
        portions = 1.0;
    chunk_size = file_size / portions;

    g_conf.chunk_size = (size_t) ceil(chunk_size);
    if (!g_conf.chunk_size || g_conf.chunk_size > (size_t)file->info.st_size)
        g_conf.chunk_size = file->info.st_size;
}


#if PARTITION_MIN_CHUNKS > 0
/** Cached long lines usually do not repay the extra partition I/O. */
static bool     short_lines(void)
{
    t_chunk chunk;
    char    *line;
    int     length;
    size_t  size = g_file->orig_size;
    size_t  count = 0;
    size_t  bytes = 0;
    size_t  junk = 0;
    int     i;

    if (size > 0x4000)
        size = 0x4000;
    for (i = 0; i < 4; ++i)
    {
        chunk.ptr = g_file->addr + (g_file->orig_size - size) / 3 * i;
        chunk.endptr = chunk.ptr + size;
        if (i)
        {
            line = memchr(chunk.ptr, '\n', size);
            if (line == NULL)
                continue;
            chunk.ptr = line + 1;
        }
        while (chunk.endptr > chunk.ptr && chunk.endptr[-1] != '\n')
            --chunk.endptr;
        while (get_next_line(&line, &length, &chunk, &junk))
        {
            bytes += length;
            ++count;
        }
    }
    return (count && bytes <= count * 2 * sizeof(t_line));
}
#endif


/** Select an initial engine before checking its exact runtime requirements. */
static void     config_dedupe(const struct memstate *memstate)
{
#if PARTITION_MIN_CHUNKS > 0
    size_t  chunks;

    if (!g_conf.chunk_size)
        return;
    chunks = g_file->orig_size / g_conf.chunk_size
        + (g_file->orig_size % g_conf.chunk_size != 0);
    if (chunks < PARTITION_MIN_CHUNKS)
    {
        DLOG1("partitions disabled: %zu estimated chunks", chunks);
        return;
    }
    if (chunks < 8 * PARTITION_MIN_CHUNKS
            && memstate->mem_available > (long long)g_conf.dedupe_memory
            && g_file->orig_size <= (size_t)memstate->mem_available - g_conf.dedupe_memory
            && !short_lines())
    {
        DLOG1("partitions disabled: cached long lines");
        return;
    }
#else
    (void)memstate;
#endif
    config_partitions();
}


/** Initialize `g_conf` variables after parsing arguments.
 */
void            config(void)
{
    struct memstate memstate;

    init_memstate(&memstate);


    config_threads();
    config_hmap_size(g_file, &memstate);
    config_chunk_size(g_file);
    g_conf.dedupe_memory = g_conf.hmap_size * sizeof(t_line);
    config_dedupe(&memstate);

    DLOG1("");
    DLOG1("--------- memstate -----------");
    DLOG1("memstate.page_size:      %d", memstate.page_size);
    DLOG1("memstate.mem_budget:     %s (%lld)",
            sizerepr(memstate.mem_budget), memstate.mem_budget);
    DLOG1("memstate.mem_available:  %s (%lld)",
            sizerepr(memstate.mem_available), memstate.mem_available);
    DLOG1("------------------------------");

    DLOG1("");
    DLOG1("---------- g_conf ------------");
    DLOG1("g_conf.infile_name:       %s", g_conf.infile_name);
    DLOG1("g_conf.outfile_name:      %s", g_conf.outfile_name);
    if (g_conf.dupfile_name != NULL) {
        DLOG1("g_conf.dupfile_name:      %s", g_conf.dupfile_name);
    } else {
        DLOG1("g_conf.dupfile_name:      (null)");
    }
    DLOG1("g_conf.threads:           %u", g_conf.threads);
    DLOG1("g_conf.line_max_size:     %u", g_conf.line_max_size);
    DLOG1("g_conf.hmap_size:         %s (%ld slots of %dbits)",
            sizerepr(g_conf.hmap_size * sizeof(t_line)),
            g_conf.hmap_size, sizeof(t_line) * 8);
    DLOG1("g_conf.chunk_size:        %s (%ld)",
            sizerepr(g_conf.chunk_size), g_conf.chunk_size);
    DLOG1("g_conf.dedupe_memory:     %zu bytes", g_conf.dedupe_memory);
    DLOG1("g_conf.partition_count:   %zu (candidate)", g_conf.partition_count);
    DLOG1("g_conf.partition_workers: %zu (maximum)", g_conf.partition_workers);
    DLOG1("legacy hash load:        %.0f%% target, %.0f%% maximum",
            HMAP_LOAD_FACTOR * 100, HMAP_MAX_LOAD * 100);
    DLOG1("g_conf.filter_printable:  %d", g_conf.filter_printable);
    DLOG1("g_conf.memlimit:          %ld", g_conf.memlimit);
    DLOG1("------------------------------");
    DLOG1("");
}
