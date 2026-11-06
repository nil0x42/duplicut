#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#include "partition.h"
#include "engine.h"
#include "chunk.h"
#include "line.h"
#include "hash.h"
#include "hmap.h"
#include "file.h"
#include "config.h"
#include "const.h"
#include "status.h"
#include "thpool.h"
#include "error.h"
#include "debug.h"
#include "bytesize.h"

#define RECORD_HEADER_MAX (10)
#define RECORD_SIZE(low)             ((uint16_t)((low) & 0xFFF))
#define RECORD_ID(low, high)         (((low) >> 12) | ((uint64_t)(high) << 52))
#define MAKE_RECORD_LOW(id, size)    (((uint64_t)(id) << 12) | ((size) & 0xFFF))
#define MAKE_RECORD_HIGH(id)         ((uint16_t)((id) >> 52))

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define RECORD_LE64(value) __builtin_bswap64(value)
# define RECORD_LE16(value) __builtin_bswap16(value)
#else
# define RECORD_LE64(value) (value)
# define RECORD_LE16(value) (value)
#endif

#define BUF_SIZE (0x4000)
#define WORKER_OVERHEAD (2 * BUF_SIZE + MAX_MAX_LINE_SIZE)
#define WORKER_MEMORY (64 * BUF_SIZE)

struct partition
{
    size_t          start;
    size_t          end;
    uint64_t        lines;
    size_t          cursor;
    size_t          buffered;
    char            *buffer;
};

struct partition_work
{
    struct partition *parts;
    size_t          count;
    size_t          next;
    size_t          memory;
    size_t          header_size;
    uint64_t        lines;
    int             fd;
    unsigned char   *duplicates;
    pthread_mutex_t mutex;
};


struct partition_input
{
    int             fd;
    size_t          offset;
    size_t          end;
    size_t          used;
    size_t          available;
    char            buffer[BUF_SIZE];
};


/** The low word packs length and index; its extension keeps all 64 index bits. */
static void     encode_record(uint8_t *header, uint64_t id, uint16_t size)
{
    uint64_t low = RECORD_LE64(MAKE_RECORD_LOW(id, size));
    uint16_t high = RECORD_LE16(MAKE_RECORD_HIGH(id));

    memcpy(header, &low, sizeof(low));
    memcpy(header + sizeof(low), &high, sizeof(high));
}


static bool     decode_record(const uint8_t *header, uint64_t *id, uint16_t *size)
{
    uint64_t low;
    uint16_t high;

    memcpy(&low, header, sizeof(low));
    memcpy(&high, header + sizeof(low), sizeof(high));
    low = RECORD_LE64(low);
    high = RECORD_LE16(high);
    *id = RECORD_ID(low, high);
    *size = RECORD_SIZE(low);
    return (high <= 0xFFF);
}


static size_t   record_header_size(uint64_t lines)
{
    uint64_t id = lines ? lines - 1 : 0;
    size_t   size = 5;

    for (id >>= 28; id; id >>= 8)
        ++size;
    return (size);
}


static void     write_partition(int fd, struct partition *part)
{
    size_t  written = 0;
    ssize_t result;

    while (written < part->buffered)
    {
        result = pwrite(fd, part->buffer + written,
                part->buffered - written, part->cursor + written);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            error("cannot write partitions: %s", result < 0 ? ERRNO : "short write");
        written += result;
    }
    part->cursor += written;
    part->buffered = 0;
}


static void     append_partition(int fd, struct partition *part,
                    const void *data, size_t size)
{
    size_t length;

    while (size)
    {
        length = BUF_SIZE - part->buffered;
        if (length > size)
            length = size;
        memcpy(part->buffer + part->buffered, data, length);
        part->buffered += length;
        data = (const char*)data + length;
        size -= length;
        if (part->buffered == BUF_SIZE)
            write_partition(fd, part);
    }
}


static void     read_partition(struct partition_input *input, void *data, size_t size)
{
    size_t  length;
    ssize_t result;

    while (size)
    {
        if (input->used == input->available)
        {
            length = input->end - input->offset;
            if (length > BUF_SIZE)
                length = BUF_SIZE;
            do {
                result = pread(input->fd, input->buffer, length, input->offset);
            } while (result < 0 && errno == EINTR);
            if (result <= 0)
                error("cannot read partitions: %s", result < 0 ? ERRNO : "unexpected EOF");
            input->offset += result;
            input->available = result;
            input->used = 0;
        }
        length = input->available - input->used;
        if (length > size)
            length = size;
        memcpy(data, input->buffer + input->used, length);
        input->used += length;
        data = (char*)data + length;
        size -= length;
    }
}


/** Keep unseen records in the same range for the next bounded pass. */
static void     clean_partition(struct partition_work *work,
                    struct partition *part, t_line *table, size_t slots,
                    char *area, size_t capacity)
{
    size_t      end = part->end;
    size_t      readpos;
    struct partition_input input;
    struct partition output;
    char        buffer[BUF_SIZE];
    size_t      used;
    size_t      filled;
    size_t      slot;
    uint64_t    id;
    uint16_t    length;
    char        line[MAX_MAX_LINE_SIZE];
    /* Width is fixed: bytes beyond each short header stay zero across passes. */
    uint8_t     header[RECORD_HEADER_MAX] = {0};

    while (end > part->start)
    {
        memset(table, 0, slots * sizeof(t_line));
        readpos = part->start;
        input = (struct partition_input){.fd = work->fd,
            .offset = part->start, .end = end};
        output = (struct partition){.cursor = part->start, .buffer = buffer};
        used = 0;
        filled = 0;
        while (readpos < end)
        {
            if (end - readpos < work->header_size)
                error("invalid partition record");
            read_partition(&input, header, work->header_size);
            if (!decode_record(header, &id, &length)
                    || id >= work->lines || !length || length > MAX_MAX_LINE_SIZE
                    || length > end - readpos - work->header_size)
                error("invalid partition record");
            read_partition(&input, line, length);
            slot = HASH(line, length) % slots;
            while (LINE_ISSET(table[slot]) && cmp_line(table[slot], line, length))
                slot = (slot + 1) % slots;
            if (LINE_ISSET(table[slot]))
                __atomic_fetch_or(&work->duplicates[id / 8],
                        (unsigned char)(1U << (id % 8)), __ATOMIC_RELAXED);
            else if (filled < slots / 2 && length <= capacity - used)
            {
                memcpy(area + used, line, length);
                table[slot] = MAKE_LINE(area + used, length);
                used += length;
                ++filled;
            }
            else
            {
                append_partition(work->fd, &output, header, work->header_size);
                append_partition(work->fd, &output, line, length);
            }
            readpos += work->header_size + length;
        }
        if (!filled)
            error("partition memory budget is too small");
        write_partition(work->fd, &output);
        DLOG2("partition at %zu: %zu input bytes, %zu deferred bytes",
                part->start, end - part->start, output.cursor - part->start);
        DLOG3("partition storage: %zu/%zu entries, %zu/%zu content bytes",
                filled, slots / 2, used, capacity);
        end = output.cursor;
    }
}


static void     partition_worker(void *arg)
{
    struct partition_work *work = arg;
    size_t      index;
    size_t      slots;
    size_t      capacity;
    t_line      *table;
    char        *area;

    slots = (work->memory - WORKER_OVERHEAD) / (2 * sizeof(t_line));
    capacity = work->memory - WORKER_OVERHEAD - slots * sizeof(t_line);
    table = malloc(slots * sizeof(t_line));
    area = malloc(capacity);
    if (table == NULL || area == NULL)
        die("cannot malloc() partition memory");
    while (true)
    {
        pthread_mutex_lock(&work->mutex);
        index = work->next++;
        pthread_mutex_unlock(&work->mutex);
        if (index >= work->count)
            break;
        DLOG2("partition %zu: %zu bytes", index,
                work->parts[index].end - work->parts[index].start);
        clean_partition(work, &work->parts[index], table, slots, area, capacity);
        set_status(TAGDUP_BYTES, work->parts[index].end - work->parts[index].start);
    }
    free(area);
    free(table);
}


/** Prepare bounded partition buffers and a maximum worker count. */
void            config_partitions(void)
{
    size_t  budget = g_conf.dedupe_memory;
    size_t  batches;
    size_t  count = 1;
    size_t  workers = g_conf.threads;

    if (budget < 4 * WORKER_OVERHEAD || !g_conf.chunk_size)
    {
        DLOG1("disk-partitions disabled: insufficient working memory");
        return;
    }
    batches = (size_t)ceil(g_file->orig_size / (budget * HMAP_LOAD_FACTOR));
#if MULTITHREADING != 1
    workers = 1;
#endif
    if (workers > budget / WORKER_MEMORY)
        workers = budget / WORKER_MEMORY;
    if (!workers)
        workers = 1;
    if (batches < 4 * workers)
        batches = 4 * workers;
    while (count / workers < batches
            && count < budget / (4 * (BUF_SIZE + sizeof(struct partition)))
            && count < g_file->orig_size / BUF_SIZE)
        count *= 2;
    if (workers > count)
        workers = count;
    g_conf.partition_count = count;
    g_conf.partition_workers = workers;
}


/** Finalize exact range sizes once the global index width is known. */
static size_t   size_partitions(struct partition_work *work, size_t contents)
{
    size_t index;
    size_t bytes;

    work->header_size = record_header_size(work->lines);
    if (contents > (size_t)LLONG_MAX
            || work->lines > ((size_t)LLONG_MAX - contents) / work->header_size)
        error("partition file is too large");
    bytes = contents + work->lines * work->header_size;
    for (index = 0; index < work->count; ++index)
        work->parts[index].end += work->parts[index].lines * work->header_size;
    return (bytes);
}


/** Count exact record ranges before creating the partition file. */
static size_t   count_partitions(struct partition_work *work)
{
    t_chunk     chunk;
    t_chunk     range;
    t_chunk     mapped;
    size_t      count = work->count;
    size_t      junk = 0;
    size_t      bytes = 0;
    size_t      index;
    uint64_t    lines = 0;
    char        *line;
    char        *base;
    int         size;

    partition_status(0, g_file->orig_size);
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    range = chunk;
    while (range.ptr < range.endptr)
    {
        chunk = get_chunk_window(&range);
        mapped = chunk;
        while (get_next_line(&line, &size, &chunk, &junk))
        {
            index = HASH(line, size) & (count - 1);
            if ((size_t)size > (size_t)LLONG_MAX - bytes)
                error("partition file is too large");
            work->parts[index].end += size;
            ++work->parts[index].lines;
            bytes += size;
            ++lines;
            if (lines % 500000 == 0)
            {
                set_status(TAGDUP_BYTES, chunk.ptr - base);
                base = chunk.ptr;
            }
        }
        release_chunk(&mapped);
        range.ptr = mapped.endptr;
    }
    set_status(TAGDUP_BYTES, g_file->orig_size - (base - g_file->addr));
    work->lines = lines;
    return (size_partitions(work, bytes));
}


/** Scatter records into fixed ranges with bounded buffers. */
static void     write_partitions(struct partition_work *work)
{
    t_chunk     chunk;
    t_chunk     range;
    t_chunk     mapped;
    size_t      count = work->count;
    size_t      index;
    size_t      position;
    size_t      junk = 0;
    uint64_t    id = 0;
    uint8_t     header[RECORD_HEADER_MAX];
    int         size;
    int         fd = work->fd;
    char        *line;
    char        *base;

    position = 0;
    for (index = 0; index < count; ++index)
    {
        work->parts[index].start = position;
        position += work->parts[index].end;
        work->parts[index].end = position;
        work->parts[index].cursor = work->parts[index].start;
    }
    partition_status(1, g_file->orig_size);
    for (index = 0; index < count; ++index)
    {
        work->parts[index].buffer = malloc(BUF_SIZE);
        if (work->parts[index].buffer == NULL)
            die("cannot malloc() partition buffer");
    }
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    range = chunk;
    while (range.ptr < range.endptr)
    {
        chunk = get_chunk_window(&range);
        mapped = chunk;
        while (get_next_line(&line, &size, &chunk, &junk))
        {
            index = HASH(line, size) & (count - 1);
            encode_record(header, id, size);
            append_partition(fd, &work->parts[index], header, work->header_size);
            append_partition(fd, &work->parts[index], line, size);
            ++id;
            if (id % 500000 == 0)
            {
                set_status(TAGDUP_BYTES, chunk.ptr - base);
                base = chunk.ptr;
            }
        }
        release_chunk(&mapped);
        range.ptr = mapped.endptr;
    }
    set_status(TAGDUP_BYTES, g_file->orig_size - (base - g_file->addr));
    for (index = 0; index < count; ++index)
    {
        write_partition(fd, &work->parts[index]);
        free(work->parts[index].buffer);
        work->parts[index].buffer = NULL;
    }
}


/** Process independent ranges with the confirmed worker budget. */
static void     dedupe_partitions(struct partition_work *work, size_t workers)
{
    threadpool  pool;
    size_t      index;

    pthread_mutex_init(&work->mutex, NULL);
    if (workers == 1)
        partition_worker(work);
    else
    {
        if ((pool = thpool_init(workers)) == NULL)
            exit(1);
        for (index = 0; index < workers; ++index)
            if (thpool_add_work(pool, partition_worker, work) != 0)
                exit(1);
        thpool_wait(pool);
        thpool_destroy(pool);
    }
    pthread_mutex_destroy(&work->mutex);
}


/** Apply the bitmap in source order, counting each occurrence once. */
static void     mark_partitions(const struct partition_work *work)
{
    t_chunk     chunk;
    t_chunk     range;
    t_chunk     mapped;
    size_t      junk = 0;
    size_t      duplicates = 0;
    uint64_t    id;
    int         size;
    char        *line;
    char        *base;

    partition_status(3, g_file->orig_size);
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    id = 0;
    range = chunk;
    while (range.ptr < range.endptr)
    {
        chunk = get_chunk_window(&range);
        mapped = chunk;
        while (get_next_line(&line, &size, &chunk, &junk))
        {
            if (work->duplicates[id / 8] & (1U << (id % 8)))
            {
                if (g_conf.dupfile_name != NULL)
                    log_duplicate(line, size);
                *line = DISABLED_LINE;
                ++duplicates;
            }
            ++id;
            if (id % 500000 == 0)
            {
                set_status(TAGDUP_BYTES, chunk.ptr - base);
                base = chunk.ptr;
            }
        }
        release_chunk(&mapped);
        range.ptr = mapped.endptr;
    }
    set_status(TAGDUP_DUPLICATES, duplicates);
    set_status(TAGDUP_BYTES, g_file->orig_size - (base - g_file->addr));
}


/** Do not reserve storage for more records than the largest range can hold. */
static void     limit_partition_memory(struct partition_work *work)
{
    size_t  records = 0;
    size_t  index;
    size_t  count;

    for (index = 0; index < work->count; ++index)
    {
        count = work->parts[index].end / (work->header_size + 1);
        if (count > records)
            records = count;
    }
    if (records < (work->memory - WORKER_OVERHEAD) / (4 * sizeof(t_line)))
        work->memory = WORKER_OVERHEAD + records * 4 * sizeof(t_line);
}


/** Check exact allocations and expose usable capacities to engine selection. */
static bool     prepare_partition_memory(struct partition_work *work,
                    size_t bytes, size_t bitmap, size_t *workers)
{
    size_t  memory = g_conf.dedupe_memory - work->count * sizeof(struct partition);
    size_t  slots;
    struct partition_estimate estimate;

    if (bitmap > memory || memory - bitmap < 4 * WORKER_OVERHEAD)
    {
        DLOG1("disk-partitions unavailable: %zu bitmap bytes, %zu memory bytes",
                bitmap, memory);
        return (engine_fallback("bitmap and worker storage do not fit the memory budget"));
    }
    memory -= bitmap;
    if (*workers > memory / WORKER_MEMORY)
        *workers = memory / WORKER_MEMORY;
    if (!*workers)
        *workers = 1;
    work->memory = memory / *workers;
    limit_partition_memory(work);
    slots = (work->memory - WORKER_OVERHEAD) / (2 * sizeof(t_line));
    estimate = (struct partition_estimate){
        .lines = work->lines, .contents = bytes - work->lines * work->header_size,
        .bytes = bytes, .count = work->count, .workers = *workers,
        .entries = slots / 2, .capacity = work->memory - WORKER_OVERHEAD - slots * sizeof(t_line),
        .bitmap = bitmap
    };
    return (prefer_partitions(&estimate));
}


static void     display_partition_config(const struct partition_work *work,
                    size_t workers, size_t bytes, size_t bitmap)
{
    size_t table = ((work->memory - WORKER_OVERHEAD) / (2 * sizeof(t_line))) * sizeof(t_line);
    char data[32];
    char entries[32];

    snprintf(data, sizeof(data), "%s", sizerepr(bytes));
    snprintf(entries, sizeof(entries), "%s", sizerepr(table));
    status_info("engine:", "\e[1mdisk-partitions\e[0m (%s); workers: \e[1m%zu\e[0m",
        g_conf.engine == ENGINE_AUTO ? "auto" : "forced", workers);
    status_info("partitions:", "\e[1m%zu\e[0m; data: %s; bitmap: %s; header: %zuB",
        work->count, data, sizerepr(bitmap), work->header_size);
    status_info("worker RAM:", "%s per worker (table, contents and scratch)", sizerepr(work->memory));
    status_info("storage:", "table: %s; contents: %s per worker; load limit: 50%%",
        entries, sizerepr(work->memory - WORKER_OVERHEAD - table));
}


/** Confirm the initial plan before partition I/O or source modification. */
bool            tag_partitions(void)
{
    struct partition_work work = {.count = g_conf.partition_count};
    size_t      workers = g_conf.partition_workers;
    size_t      bytes;
    size_t      bitmap;

    if (!work.count)
        return (false);
    work.parts = calloc(work.count, sizeof(struct partition));
    if (work.parts == NULL)
        die("cannot calloc() partitions");
    bytes = count_partitions(&work);
    bitmap = work.lines / 8 + (work.lines % 8 != 0);
    if (!bytes)
    {
        DLOG1("dedupe engine: disk-partitions (no accepted lines)");
        status_info("engine:", "disk-partitions (%s); no accepted lines",
            g_conf.engine == ENGINE_AUTO ? "auto" : "forced");
        free(work.parts);
        return (true);
    }
    if (!prepare_partition_memory(&work, bytes, bitmap, &workers))
    {
        free(work.parts);
        partition_status(-1, 0);
        return (false);
    }
    work.fd = open_tmpfile(bytes);
    if (work.fd < 0)
    {
        DLOG1("disk-partitions unavailable: temporary file: %s", ERRNO);
        free(work.parts);
        partition_status(-1, 0);
        return (engine_fallback("cannot create the partition file on a writable disk volume"));
    }
    if (ftruncate(work.fd, bytes) < 0)
        error("cannot prepare partition file: %s", ERRNO);
    DLOG1("dedupe engine: disk-partitions");
    DLOG1("partitions: %zu ranges, %zu workers, %zu bytes, %zu bitmap bytes, %zu-byte headers",
            work.count, workers, bytes, bitmap, work.header_size);
    DLOG1("partition records: %llu accepted occurrences, %zu content bytes",
            (unsigned long long)work.lines, bytes - work.lines * work.header_size);
    DLOG1("partition memory: %zu budget bytes, %zu metadata bytes, %zu bytes per worker",
            g_conf.dedupe_memory, work.count * sizeof(struct partition), work.memory);
    display_partition_config(&work, workers, bytes, bitmap);
    write_partitions(&work);
    work.duplicates = calloc(bitmap, 1);
    if (work.duplicates == NULL)
        die("cannot calloc() duplicate bitmap");
    partition_status(2, bytes);
    dedupe_partitions(&work, workers);
    mark_partitions(&work);
    if (close(work.fd) < 0)
        error("cannot close partitions: %s", ERRNO);
    free(work.duplicates);
    free(work.parts);
    return (true);
}
