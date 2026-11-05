#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "const.h"
#include "config.h"
#include "memstate.h"
#include "chunk.h"
#include "line.h"
#include "file.h"
#include "error.h"
#include "debug.h"
#include "bytesize.h"
#include "engine.h"

/** Count accepted lines within an aligned sample, including skipped bytes. */
static void     sample_chunk(t_chunk chunk, struct line_sample *sample)
{
    char    *line;
    int     length;
    size_t  bytes = chunk.endptr - chunk.ptr;
    size_t  count = 0;
    size_t  junk = 0;
    double  mean;

    while (get_next_line(&line, &length, &chunk, &junk))
    {
        sample->contents += length;
        ++count;
    }
    sample->bytes += bytes;
    sample->lines += count;
    if (!count)
    {
        DLOG1("line sample: %zu bytes, no accepted lines", bytes);
        return;
    }
    mean = (double)bytes / count;
    if (!sample->min_size || mean < sample->min_size)
        sample->min_size = mean;
    DLOG1("line sample: %zu bytes, %zu accepted lines, %.3f bytes/line",
            bytes, count, mean);
}


/** Inspect at most 64 KiB, without counting partial boundary lines. */
static void     config_sample(struct line_sample *sample)
{
    t_chunk chunk;
    char    *next;
    size_t  size = g_file->orig_size;
    size_t  offset;
    int     i;

    if (size && size <= 0x10000)
        sample_chunk((t_chunk){g_file->addr, g_file->addr + size}, sample);
    else if (size)
    {
        for (i = 0; i < 4; ++i)
        {
            offset = i == 3 ? size - 0x4000 : (size - 0x4000) / 3 * i;
            chunk.ptr = g_file->addr + offset;
            chunk.endptr = chunk.ptr + 0x4000;
            if (i)
            {
                next = memchr(chunk.ptr, '\n', 0x4000);
                if (next == NULL)
                    continue;
                chunk.ptr = next + 1;
            }
            if (chunk.endptr != g_file->addr + size)
                while (chunk.endptr > chunk.ptr && chunk.endptr[-1] != '\n')
                    --chunk.endptr;
            sample_chunk(chunk, sample);
        }
    }
    if (!sample->lines)
    {
        sample->min_size = AVG_LINE_BYTES;
        DLOG1("line sample unavailable: using %d bytes/line", AVG_LINE_BYTES);
    }
    DLOG1("line estimate: %.3f minimum bytes/line, %.3f mean bytes/line, %.3f mean content bytes, %zu sampled bytes",
            sample->min_size, sample->lines
                ? (double)sample->bytes / sample->lines : sample->min_size,
            sample->lines ? (double)sample->contents / sample->lines : 0, sample->bytes);
}


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


/** Round a capacity to a prime, upwards or downwards in steps of two. */
static long     get_prime(long n, int step)
{
    int i;

    n = step > 0 ? n | 1 : (n - 1) | 1;
    while (n > 2)
    {
        i = 3;
        while (i <= n / i && n % i)
            i += 2;
        if (i > n / i)
            return (n);
        n += step;
    }
    return (2);
}


/** Configuration --> g_conf.hmap_size
 * Use the densest sample without exceeding the allocation budget.
 */
static void     config_hmap_size(struct file *file, const struct line_sample *sample)
{
    double      max_size;
    double      hmap_size;

    hmap_size = (double)file->info.st_size;
    hmap_size /= HMAP_LOAD_FACTOR * sample->min_size;
    if (hmap_size < 10000.0)
        hmap_size = 10000.0;

    max_size = g_conf.dedupe_memory;
    max_size /= sizeof(t_line);
    if (hmap_size > max_size)
        hmap_size = max_size;

    if (hmap_size < max_size)
    {
        g_conf.hmap_size = (size_t)get_prime((long)ceil(hmap_size), 2);
        if (g_conf.hmap_size <= (size_t)max_size)
            return;
    }
    g_conf.hmap_size = (size_t)get_prime((long)max_size, -2);
}


/** Configuration --> g_conf.chunk_size
 * Use mean density; the insertion limit handles locally denser regions.
 * Balance chunks with the existing nearest-count rounding.
 * get_next_chunk() extends chunk boundaries to the next newline.
 */
static void     config_chunk_size(struct file *file, const struct line_sample *sample)
{
    double      file_size;
    double      portions;
    double      chunk_size;
    double      max_size;

    file_size = (double) file->info.st_size;
    chunk_size = (double) g_conf.hmap_size;
    chunk_size *= HMAP_LOAD_FACTOR * (sample->lines
        ? (double)sample->bytes / sample->lines : sample->min_size);
    max_size = g_conf.dedupe_memory * (1 + HMAP_LOAD_FACTOR)
        - g_conf.hmap_size * sizeof(t_line);
    if (chunk_size > max_size)
        chunk_size = max_size;
    portions = round(file_size / chunk_size);
    if (portions < 1.0)
        portions = 1.0;
    chunk_size = file_size / portions;
    /* Keep the previous working-set envelope, including its rounding margin. */
    max_size += g_conf.dedupe_memory * HMAP_LOAD_FACTOR / 2;
    if (chunk_size > max_size)
        chunk_size = max_size;

    g_conf.chunk_size = (size_t) ceil(chunk_size);
    if (!g_conf.chunk_size || g_conf.chunk_size > (size_t)file->info.st_size)
        g_conf.chunk_size = file->info.st_size;
}


/** Divide remaining mapped memory between readers and cleanup buffers. */
static void     config_mmap_window(const struct memstate *memstate)
{
    size_t  budget = memstate->mem_budget;
    size_t  table = g_conf.hmap_size * sizeof(t_line);
    size_t  allocation = g_conf.partition_count ? g_conf.dedupe_memory : table;
    size_t  used = table + g_conf.chunk_size;
    size_t  readers = 3;

#if MULTITHREADING == 1
    readers = g_conf.threads + 2;
#endif
    g_conf.page_size = memstate->page_size;
    g_conf.mmap_window = 0;
    if (g_file->orig_size <= budget - allocation)
        return;
    g_conf.mmap_window = budget > used ? (budget - used) / readers : 0;
    if (g_conf.mmap_window < g_conf.page_size)
        g_conf.mmap_window = g_conf.page_size;
}


/** Display common budgets and the initial plan for each engine. */
static void     display_config(const struct memstate *memstate)
{
#if DEBUG >= 1
    DLOG1("");
    DLOG1("--------- memstate -----------");
    DLOG1("memstate.page_size:      %d", memstate->page_size);
    DLOG1("memstate.mem_budget:     %s (%lld)",
            sizerepr(memstate->mem_budget), memstate->mem_budget);
    DLOG1("memstate.mem_available:  %s (%lld)",
            sizerepr(memstate->mem_available), memstate->mem_available);
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
    DLOG1("g_conf.engine:            %s", engine_name(g_conf.engine));
    DLOG1("g_conf.threads:           %u", g_conf.threads);
    DLOG1("multithreading:          %s", MULTITHREADING == 1 ? "enabled" : "disabled");
    DLOG1("g_conf.line_max_size:     %u", g_conf.line_max_size);
    DLOG1("g_conf.hmap_size:         %s (%zu slots of %zu bits, mmap-chunks)",
            sizerepr(g_conf.hmap_size * sizeof(t_line)),
            g_conf.hmap_size, sizeof(t_line) * 8);
    DLOG1("g_conf.chunk_size:        %s (%ld)",
            sizerepr(g_conf.chunk_size), g_conf.chunk_size);
    DLOG1("g_conf.mmap_window:       %zu bytes", g_conf.mmap_window);
    DLOG1("g_conf.dedupe_memory:     %zu bytes", g_conf.dedupe_memory);
    DLOG1("g_conf.partition_count:   %zu (candidate)", g_conf.partition_count);
    DLOG1("g_conf.partition_workers: %zu (maximum)", g_conf.partition_workers);
    DLOG1("mmap-chunks hash load:    %.0f%% target, %.0f%% maximum",
            HMAP_LOAD_FACTOR * 100, HMAP_MAX_LOAD * 100);
    DLOG1("mmap-chunks table + chunk: %.0f target bytes, %.0f maximum bytes (estimated)",
            g_conf.dedupe_memory * (1 + HMAP_LOAD_FACTOR),
            g_conf.dedupe_memory * (1 + 1.5 * HMAP_LOAD_FACTOR));
    DLOG1("disk-partitions table load: %.0f%% maximum",
            HMAP_LOAD_FACTOR * 100);
    DLOG1("g_conf.filter_printable:  %d", g_conf.filter_printable);
    DLOG1("g_conf.memlimit:          %ld", g_conf.memlimit);
    DLOG1("------------------------------");
    DLOG1("");
    DLOG1("engine candidate: %s", g_conf.partition_count
            ? "disk-partitions" : "mmap-chunks");
#else
    (void)memstate;
#endif
}


/** Initialize `g_conf` variables after parsing arguments. */
void            config(void)
{
    struct memstate memstate;
    struct line_sample sample = {0};

    init_memstate(&memstate);
    config_threads();
    g_conf.dedupe_memory = (size_t)(memstate.mem_budget * HMAP_MAX_SIZE);
    config_sample(&sample);
    config_hmap_size(g_file, &sample);
    config_chunk_size(g_file, &sample);
    config_engine(&memstate, &sample);
    config_mmap_window(&memstate);
    display_config(&memstate);
}
