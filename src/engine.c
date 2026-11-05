#include <math.h>
#include "engine.h"
#include "config.h"
#include "const.h"
#include "file.h"
#include "line.h"
#include "memstate.h"
#include "partition.h"
#include "debug.h"
#include "error.h"

#ifndef PARTITION_MIN_CHUNKS
# define PARTITION_MIN_CHUNKS 16
#endif


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
    return (g_conf.threads > 1 && chunks <= g_conf.threads
        && g_file->orig_size / sample->bytes >= g_conf.threads
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
    if (chunks < 8 * PARTITION_MIN_CHUNKS && cached_long_input(memory, sample))
    {
        DLOG1("disk-partitions disabled: cached long lines");
        return (false);
    }
    DLOG1("disk-partitions candidate: %zu chunks for %u threads", chunks, g_conf.threads);
    return (true);
}

#endif


/** Configure the requested engine, retaining resource checks for forced runs. */
void            config_engine(const struct memstate *memory, const struct line_sample *sample)
{
    g_conf.partition_count = 0;
    g_conf.partition_workers = 0;
    if (g_conf.engine == ENGINE_MMAP_CHUNKS)
        return;
#if PARTITION_MIN_CHUNKS > 0
    if (g_conf.engine == ENGINE_AUTO && !initial_partitions(memory, sample))
        return;
#else
    (void)memory;
    (void)sample;
#endif
    config_partitions();
    if (!g_conf.partition_count && g_conf.engine == ENGINE_DISK_PARTITIONS)
        engine_fallback("memory budget is below the minimum partition working space");
}


/** Assume every occurrence is unique when bounding private overflow passes. */
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
    double partition_bytes;
    double chunk_bytes;

    if (!PARTITION_MIN_CHUNKS || g_conf.engine == ENGINE_DISK_PARTITIONS
            || estimate->bitmap <= g_conf.dedupe_memory / 2)
        return (true);
    passes = private_passes(estimate);
    chunks = ceil((double)g_file->orig_size / g_conf.chunk_size);
    partition_bytes = 3.0 * g_file->orig_size + (passes + 1) * estimate->bytes;
    chunk_bytes = (double)g_file->orig_size * (chunks + 1) / 2;
    DLOG1("large bitmap estimate: %.0f private passes, %.0f partition bytes, %.0f mmap-chunks bytes",
            passes, partition_bytes, chunk_bytes);
    if (partition_bytes >= chunk_bytes)
    {
        DLOG1("disk-partitions disabled: insufficient private storage for a large bitmap");
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
    return (false);
}
