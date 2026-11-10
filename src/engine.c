#include <math.h>
#include "engine.h"
#include "config.h"
#include "const.h"
#include "file.h"
#include "line.h"
#include "memstate.h"
#include "debug.h"
#include "error.h"
#include "status.h"

#ifndef PARTITION_MIN_CHUNKS
# define PARTITION_MIN_CHUNKS 16
#endif

/* Leave a margin over fixed source scans and temporary I/O. */
#define PARTITION_MIN_BATCHES 8


const char      *engine_name(enum e_engine engine)
{
    static const char *names[] = {"auto", "mmap-chunks", "disk-partitions"};

    return (names[engine]);
}


#if PARTITION_MIN_CHUNKS > 0

/** Few short-line chunks leave insertion serial and lookup workers idle. */
static bool     parallel_short_input(const struct line_sample *sample, size_t chunks)
{
#if MULTITHREADING == 1
    size_t workers = g_conf.partition_workers;

    return (workers > 1 && chunks <= workers
        && g_file->orig_size / sample->bytes >= workers
        && (double)sample->contents / sample->lines <= 2 * sizeof(t_line));
#else
    (void)sample;
    (void)chunks;
    return (false);
#endif
}


/** Cached long lines need more scans to justify copying complete contents. */
static bool     cached_long_input(const struct memstate *memory, const struct line_sample *sample)
{
    size_t table = g_conf.hmap_size * sizeof(t_line);

    return (memory->mem_available > (long long)table
        && g_file->orig_size <= (size_t)memory->mem_available - table
        && (double)sample->contents / sample->lines > 2 * sizeof(t_line));
}


/** Cached lookups can run concurrently; partition scans and writes remain serial. */
static size_t   lookup_batches(size_t chunks)
{
    size_t workers = 1;

#if MULTITHREADING == 1
    workers = g_conf.threads;
#endif
    return (chunks / workers + (chunks % workers != 0));
}


/** Prefer partitions only when repeated scans or parallel insertion justify them. */
static bool     initial_partitions(const struct memstate *memory, const struct line_sample *sample)
{
    size_t chunks;

    if (!g_conf.chunk_size || !sample->lines)
    {
        DLOG1("disk-partitions disabled: no usable line sample");
        return (false);
    }
    chunks = g_file->orig_size / g_conf.chunk_size
        + (g_file->orig_size % g_conf.chunk_size != 0);
    if (chunks < PARTITION_MIN_CHUNKS && !parallel_short_input(sample, chunks))
    {
        DLOG1("disk-partitions disabled: %zu estimated chunks", chunks);
        return (false);
    }
    if (cached_long_input(memory, sample) && lookup_batches(chunks) < PARTITION_MIN_BATCHES)
    {
        DLOG1("disk-partitions disabled: cached long lines, %zu lookup batches",
                lookup_batches(chunks));
        return (false);
    }
    DLOG1("disk-partitions candidate: %zu chunks, %zu usable workers (%u requested)",
            chunks, g_conf.partition_workers, g_conf.threads);
    return (true);
}

#endif


/** Configure the requested engine, retaining resource checks for forced runs. */
void            config_engine(const struct memstate *memory, const struct line_sample *sample)
{
    if (g_conf.engine == ENGINE_MMAP_CHUNKS)
        return;
    if (!g_conf.partition_count)
    {
        if (g_conf.engine == ENGINE_DISK_PARTITIONS)
            engine_fallback("memory budget is below the minimum partition working space");
        return;
    }
#if PARTITION_MIN_CHUNKS > 0
    if (g_conf.engine == ENGINE_AUTO && !initial_partitions(memory, sample))
    {
        g_conf.partition_count = 0;
        g_conf.partition_workers = 0;
        return;
    }
#else
    (void)memory;
    (void)sample;
#endif
}


/** Estimate private overflow passes without assuming a duplicate ratio. */
static double   private_passes(const struct partition_estimate *estimate)
{
    double entries = (double)estimate->lines / estimate->count / estimate->entries;
    double contents = (double)estimate->contents / estimate->count / estimate->capacity;

    return (ceil(entries > contents ? entries : contents));
}


/** A large bitmap is useful only if its remaining storage avoids excessive scans. */
bool            prefer_partitions(const struct partition_estimate *estimate)
{
    double passes;
    double chunks;
    double partition_work;
    double chunk_work;

    if (!PARTITION_MIN_CHUNKS || g_conf.engine == ENGINE_DISK_PARTITIONS
            || estimate->bitmap <= g_conf.dedupe_memory / 2)
        return (true);
    passes = private_passes(estimate);
    chunks = ceil((double)g_file->orig_size / g_conf.chunk_size);
    partition_work = 3.0 * g_file->orig_size + (passes + 1) * estimate->bytes;
    chunk_work = (double)g_file->orig_size * (chunks + 1) / 2;
    DLOG1("large bitmap estimate: %.0f private passes, %zu workers, %.0f partition work, %.0f mmap-chunks work",
            passes, estimate->workers, partition_work, chunk_work);
    if (partition_work >= chunk_work)
    {
        DLOG1("disk-partitions disabled: insufficient private storage for a large bitmap");
        status_info("fallback:", "mmap-chunks: insufficient private storage for a large bitmap");
        return (false);
    }
    return (true);
}


/** Automatic selection may fall back; a forced engine must fail explicitly. */
bool            engine_fallback(const char *reason)
{
    if (g_conf.engine == ENGINE_DISK_PARTITIONS)
        error("disk-partitions engine unavailable: %s", reason);
    DLOG1("falling back to mmap-chunks: %s", reason);
    status_info("fallback:", "mmap-chunks: %s", reason);
    return (false);
}
