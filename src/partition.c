#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "partition.h"
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
#include "meminfo.h"

#ifndef PARTITION_MIN_CHUNKS
# define PARTITION_MIN_CHUNKS 16
#endif
#define RECORD_HEADER_SIZE (10)
#define BUF_SIZE (0x4000)
#define WORKER_OVERHEAD (2 * BUF_SIZE + MAX_MAX_LINE_SIZE)
#define WORKER_MEMORY (64 * BUF_SIZE)

struct partition
{
    size_t          start;
    size_t          end;
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
            read_partition(&input, &id, sizeof(id));
            read_partition(&input, &length, sizeof(length));
            if (id >= work->lines || !length || length > MAX_MAX_LINE_SIZE
                    || length > end - readpos - RECORD_HEADER_SIZE)
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
                append_partition(work->fd, &output, &id, sizeof(id));
                append_partition(work->fd, &output, &length, sizeof(length));
                append_partition(work->fd, &output, line, length);
            }
            readpos += RECORD_HEADER_SIZE + length;
        }
        if (!filled)
            error("partition memory budget is too small");
        write_partition(work->fd, &output);
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
        clean_partition(work, &work->parts[index], table, slots, area, capacity);
        set_status(TAGDUP_BYTES, work->parts[index].end - work->parts[index].start);
    }
    free(area);
    free(table);
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


/** Partition complete contents so comparisons never reread distant source lines. */
bool            tag_partitions(void)
{
    struct partition_work work = {0};
    t_chunk     chunk;
    size_t      budget = g_conf.hmap_size * sizeof(t_line);
    size_t      count = 1;
    size_t      chunks;
    size_t      workers;
    size_t      memory;
    size_t      junk = 0;
    size_t      bytes = 0;
    size_t      bitmap;
    size_t      index;
    size_t      position;
    size_t      duplicates = 0;
    size_t      filtered;
    uint64_t    lines = 0;
    uint64_t    id = 0;
    uint16_t    length;
    int         size;
    int         fd;
    char        *line;
    char        *base;
    threadpool  pool;
#if PARTITION_MIN_CHUNKS > 0
    long long   available;
#endif

    if (budget < 4 * WORKER_OVERHEAD || !g_conf.chunk_size)
        return (false);
    chunks = g_file->orig_size / g_conf.chunk_size
        + (g_file->orig_size % g_conf.chunk_size != 0);
#if PARTITION_MIN_CHUNKS > 0
    if (chunks < PARTITION_MIN_CHUNKS)
        return (false);
    available = meminfo(MEMAVAILABLE);
    if (chunks < 8 * PARTITION_MIN_CHUNKS
            && available > 0 && (size_t)available > budget
            && g_file->orig_size <= (size_t)available - budget && !short_lines())
        return (false);
#endif
    workers = g_conf.threads;
#if MULTITHREADING != 1
    workers = 1;
#endif
    if (workers > budget / WORKER_MEMORY)
        workers = budget / WORKER_MEMORY;
    if (!workers)
        workers = 1;
    while (count / workers < chunks
            && count < budget / (4 * (BUF_SIZE + sizeof(struct partition))))
        count *= 2;
    partition_status(0, g_file->orig_size);
    work.parts = calloc(count, sizeof(struct partition));
    if (work.parts == NULL)
        die("cannot calloc() partitions");
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    while (get_next_line(&line, &size, &chunk, &junk))
    {
        index = HASH(line, size) & (count - 1);
        if ((size_t)size + RECORD_HEADER_SIZE > (size_t)LLONG_MAX - bytes)
            error("partition file is too large");
        work.parts[index].end += RECORD_HEADER_SIZE + size;
        bytes += RECORD_HEADER_SIZE + size;
        ++lines;
        if (lines % 500000 == 0)
        {
            set_status(TAGDUP_BYTES, chunk.ptr - base);
            base = chunk.ptr;
        }
    }
    filtered = junk;
    work.lines = lines;
    bitmap = lines / 8 + (lines % 8 != 0);
    memory = budget - count * sizeof(struct partition);
    if (bitmap > budget / 2 || bitmap > memory
            || memory - bitmap < 4 * WORKER_OVERHEAD || !bytes)
    {
        free(work.parts);
        partition_status(-1, 0);
        return (false);
    }
    memory -= bitmap;
    if (workers > memory / WORKER_MEMORY)
        workers = memory / WORKER_MEMORY;
    if (!workers)
        workers = 1;
    position = 0;
    for (index = 0; index < count; ++index)
    {
        work.parts[index].start = position;
        position += work.parts[index].end;
        work.parts[index].end = position;
        work.parts[index].cursor = work.parts[index].start;
    }
    fd = open_tmpfile(bytes);
    if (fd < 0)
    {
        free(work.parts);
        partition_status(-1, 0);
        return (false);
    }
    if (ftruncate(fd, bytes) < 0)
        error("cannot prepare partition file: %s", ERRNO);
    DLOG1("partitions: %zu ranges, %zu workers, %zu bytes, %zu bitmap bytes",
            count, workers, bytes, bitmap);
    work.fd = fd;
    partition_status(1, g_file->orig_size);
    for (index = 0; index < count; ++index)
    {
        work.parts[index].buffer = malloc(BUF_SIZE);
        if (work.parts[index].buffer == NULL)
            die("cannot malloc() partition buffer");
    }
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    junk = 0;
    while (get_next_line(&line, &size, &chunk, &junk))
    {
        index = HASH(line, size) & (count - 1);
        length = size;
        append_partition(fd, &work.parts[index], &id, sizeof(id));
        append_partition(fd, &work.parts[index], &length, sizeof(length));
        append_partition(fd, &work.parts[index], line, size);
        ++id;
        if (id % 500000 == 0)
        {
            set_status(TAGDUP_BYTES, chunk.ptr - base);
            base = chunk.ptr;
        }
    }
    for (index = 0; index < count; ++index)
    {
        write_partition(fd, &work.parts[index]);
        free(work.parts[index].buffer);
        work.parts[index].buffer = NULL;
    }
    work.duplicates = calloc(bitmap, 1);
    if (work.duplicates == NULL)
        die("cannot calloc() duplicate bitmap");
    partition_status(2, bytes);
    work.count = count;
    work.memory = memory / workers;
    pthread_mutex_init(&work.mutex, NULL);
    if (workers == 1)
        partition_worker(&work);
    else
    {
        if ((pool = thpool_init(workers)) == NULL)
            exit(1);
        for (index = 0; index < workers; ++index)
            if (thpool_add_work(pool, partition_worker, &work) != 0)
                exit(1);
        thpool_wait(pool);
        thpool_destroy(pool);
    }
    pthread_mutex_destroy(&work.mutex);
    partition_status(3, g_file->orig_size);
    chunk.ptr = g_file->addr;
    chunk.endptr = g_file->addr + g_file->orig_size;
    base = chunk.ptr;
    junk = 0;
    id = 0;
    while (get_next_line(&line, &size, &chunk, &junk))
    {
        if (work.duplicates[id / 8] & (1U << (id % 8)))
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
    set_status(TAGDUP_DUPLICATES, duplicates);
    set_status(TAGDUP_JUNK_LINES, filtered);
    set_status(TAGDUP_BYTES, g_file->orig_size - (base - g_file->addr));
    if (close(fd) < 0)
        error("cannot close partitions: %s", ERRNO);
    free(work.duplicates);
    free(work.parts);
    return (true);
}
