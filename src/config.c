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
 * Used to settle `hmap` size with a prime number value,
 * ensuring an optimal hash repartition among the table.
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
 * Use `file` size and `memstate` to determine optimal size for `hmap`.
 */
static void     config_hmap_size(struct file *file, struct memstate *memstate)
{
    double      max_size;
    double      hmap_size;

    hmap_size = (double)file->info.st_size;
    hmap_size /= HMAP_LOAD_FACTOR * MEDIUM_LINE_BYTES;
    if (hmap_size < 10000.0)
        hmap_size = 10000.0;

    max_size = memstate->mem_available * HMAP_MAX_SIZE;
    max_size /= sizeof(t_line);
    if (hmap_size > max_size)
        hmap_size = max_size;

    g_conf.hmap_size = (size_t) get_prev_prime((long)hmap_size);
}


/** Configuration --> g_conf.chunk_size
 * Determine the size of a chunk, which must be a divisor of `file->size`.
 */
static void     config_chunk_size(struct file *file)
{
    double      file_size;
    double      portions;
    double      chunk_size;

    file_size = (double) file->info.st_size;
    chunk_size = (double) g_conf.hmap_size;
    chunk_size *= HMAP_LOAD_FACTOR * MEDIUM_LINE_BYTES;
    portions = round(file_size / chunk_size);
    if (portions < 1.0)
        portions = 1.0;
    chunk_size = file_size / portions;

    g_conf.chunk_size = (size_t) ceil(chunk_size);
    if (!g_conf.chunk_size || g_conf.chunk_size > (size_t)file->info.st_size)
        g_conf.chunk_size = file->info.st_size;
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

    DLOG1("");
    DLOG1("--------- memstate -----------");
    DLOG1("memstate.page_size:      %d", memstate.page_size);
    DLOG1("memstate.mem_available:  %s (%lld)",
            sizerepr(memstate.mem_available), memstate.mem_available);
    DLOG1("------------------------------");

    DLOG1("");
    DLOG1("---------- g_conf ------------");
    DLOG1("g_conf.infile_name:       %s", g_conf.infile_name);
    DLOG1("g_conf.outfile_name:      %s", g_conf.outfile_name);
    DLOG1("g_conf.threads:           %u", g_conf.threads);
    DLOG1("g_conf.line_max_size:     %u", g_conf.line_max_size);
    DLOG1("g_conf.hmap_size:         %s (%ld slots of %dbits)",
            sizerepr(g_conf.hmap_size * sizeof(t_line)),
            g_conf.hmap_size, sizeof(t_line) * 8);
    DLOG1("g_conf.chunk_size:        %s (%ld)",
            sizerepr(g_conf.chunk_size), g_conf.chunk_size);
    DLOG1("g_conf.filter_printable:  %d", g_conf.filter_printable);
    DLOG1("g_conf.memlimit:          %ld", g_conf.memlimit);
    DLOG1("------------------------------");
    DLOG1("");
}
