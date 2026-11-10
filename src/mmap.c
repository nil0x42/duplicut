#include <stdlib.h>
#include <string.h>
#include "mmap.h"
#include "const.h"
#include "config.h"
#include "thpool.h"
#include "file.h"
#include "chunk.h"
#include "line.h"
#include "hmap.h"
#include "hash.h"
#include "bytesize.h"
#include "status.h"
#include "error.h"
#include "debug.h"


static int      count_chunks(void)
{
    t_chunk     chunk = {
        .ptr = NULL,
        .endptr = NULL
    };
    int         result = 0;

    while (get_next_chunk(&chunk, g_file))
        result++;
    return (result);
}


/** Disable lines on `chunk` that already exist in `hmap`
 * by tagging them with 'DISABLED_LINE'
 * Main work for threads in thread pool.
 */
static void     cleanout_chunk(void *arg)
{
    t_chunk *chunk = arg;
    t_chunk range;
    t_chunk mapped;
    char    *line_ptr;
    int     line_sz;
    long    slot;
    char    *base_ptr;
    int     i;
    long    duplicates;
    const int has_dupfile = (g_conf.dupfile_name != NULL);

    duplicates = 0;
    i = 0;
    base_ptr = chunk->ptr;
    size_t junk_lines = 0;
    range = *chunk;
    while (range.ptr < range.endptr)
    {
        *chunk = get_chunk_window(&range);
        mapped = *chunk;
        while (get_next_line(&line_ptr, &line_sz, chunk, &junk_lines))
        {
            slot = HASH(line_ptr, line_sz) % g_hmap.size;
            while (LINE_ISSET(g_hmap.ptr[slot]))
            {
                if (cmp_line(g_hmap.ptr[slot], line_ptr, line_sz) == 0)
                {
                    if (has_dupfile) {
                        log_duplicate(line_ptr, line_sz);
                    }
                    *line_ptr = DISABLED_LINE;
                    ++duplicates;
                    break;
                }
                /* open addressing with linear probing */
                slot = (slot + 1) % g_hmap.size;
            }
            i++;
            if (i == 500000) {
                set_status(TAGDUP_BYTES, (size_t)(chunk->ptr - base_ptr));
                base_ptr = chunk->ptr;
                i = 0;
            }
        }
        release_chunk(&mapped);
        range.ptr = mapped.endptr;
    }
    set_status(TAGDUP_BYTES, (size_t)(range.endptr - base_ptr));
    set_status(TAGDUP_DUPLICATES, (size_t)duplicates);
    free(chunk);
    update_status(CTASK_DONE);
}


/** Account for the remainder, next population and following chunks. */
static void     add_split_tasks(const t_chunk *parent)
{
    t_chunk chunk = *parent;
    size_t  tasks = 2;

    while (get_next_chunk(&chunk, g_file))
        ++tasks;
    set_status(CTASK_EXTRA, tasks);
}


#if MULTITHREADING == 1

/** Queue the remainder and following chunks against the current table.
 */
static void     tag_subchunks(threadpool *thpool, const t_chunk *parent,
                    bool populated)
{
    t_chunk     chunk;
    t_chunk     *heap_chunk;

    memcpy(&chunk, parent, sizeof(t_chunk));
    do
    {
        if (chunk.ptr == chunk.endptr)
            continue;
        if (!populated)
        {
            update_status(CTASK_DONE);
            continue;
        }
        if (*thpool == NULL)
        {
            /* thpool already prints error unless DISABLE_PRINT is defined */
            if ((*thpool = thpool_init(g_conf.threads)) == NULL)
                exit(1);
        }
        heap_chunk = malloc(sizeof(t_chunk));
        if (heap_chunk == NULL)
            die("cannot malloc() heap_chunk");
        memcpy(heap_chunk, &chunk, sizeof(t_chunk));
        /* thpool already prints error unless DISABLE_PRINT is defined */
        if (thpool_add_work(*thpool, cleanout_chunk, heap_chunk) != 0)
            exit(1);
    } while (get_next_chunk(&chunk, g_file));
}

/** For each chunk, load it into hmap and remove all duplicates
 * in following chunks through `tag_subchunks()`.
 */
static void     tag_main_chunks(void)
{
    threadpool  thpool = NULL;
    t_chunk     main_chunk = {
        .ptr = NULL,
        .endptr = NULL
    };

    bool        populated;
    t_chunk     indexed;

    while (get_next_chunk(&main_chunk, g_file))
    {
        do
        {
            indexed = main_chunk;
            populated = populate_hmap(&main_chunk);
            indexed.endptr = main_chunk.ptr;
            if (main_chunk.ptr < main_chunk.endptr)
                add_split_tasks(&main_chunk);
            update_status(CTASK_DONE);
            tag_subchunks(&thpool, &main_chunk, populated);
            if (thpool != NULL)
                thpool_wait(thpool);
            release_chunk(&indexed);
        } while (main_chunk.ptr < main_chunk.endptr);
        update_status(CHUNK_DONE);
    }

    thpool_destroy(thpool);
}


#else /* MULTITHREADING != 1 */

/** Check the remainder and following chunks against the current table.
 */
static void     tag_subchunks(const t_chunk *parent, bool populated)
{
    t_chunk     chunk;
    t_chunk     *heap_chunk;

    memcpy(&chunk, parent, sizeof(t_chunk));
    do
    {
        if (chunk.ptr == chunk.endptr)
            continue;
        if (!populated)
        {
            update_status(CTASK_DONE);
            continue;
        }
        heap_chunk = malloc(sizeof(t_chunk));
        if (heap_chunk == NULL)
            die("cannot malloc() heap_chunk");
        memcpy(heap_chunk, &chunk, sizeof(t_chunk));
        cleanout_chunk(heap_chunk);
    } while (get_next_chunk(&chunk, g_file));
}

/** For each chunk, load it into hmap and remove all duplicates
 * in following chunks through `tag_subchunks()`.
 */
static void     tag_main_chunks(void)
{
    t_chunk     main_chunk = {
        .ptr = NULL,
        .endptr = NULL
    };

    bool        populated;
    t_chunk     indexed;

    while (get_next_chunk(&main_chunk, g_file))
    {
        do
        {
            indexed = main_chunk;
            populated = populate_hmap(&main_chunk);
            indexed.endptr = main_chunk.ptr;
            if (main_chunk.ptr < main_chunk.endptr)
                add_split_tasks(&main_chunk);
            update_status(CTASK_DONE);
            tag_subchunks(&main_chunk, populated);
            release_chunk(&indexed);
        } while (main_chunk.ptr < main_chunk.endptr);
        update_status(CHUNK_DONE);
    }
}

#endif


static void     display_mmap_config(void)
{
    status_info("engine:", "\e[1mmmap-chunks\e[0m (%s); lookup threads: up to \e[1m%u\e[0m",
        g_conf.engine == ENGINE_AUTO ? "auto" : "forced",
        MULTITHREADING == 1 ? g_conf.threads : 1);
    status_info("table:", "\e[1m%s\e[0m; target load: %.0f%%, limit: %.0f%%",
        sizerepr(g_conf.hmap_size * sizeof(t_line)),
        HMAP_LOAD_FACTOR * 100, HMAP_MAX_LOAD * 100);
    status_info("chunks:", "\e[1m%zu\e[0m estimated; nominal size: \e[1m%s\e[0m",
        g_file->orig_size / g_conf.chunk_size + (g_file->orig_size % g_conf.chunk_size != 0),
        sizerepr(g_conf.chunk_size));
}


void            tag_chunks(void)
{
    DLOG1("dedupe engine: mmap-chunks");
    update_status(CHUNK_START);
    set_status(CHUNK_COUNT, count_chunks());
    display_mmap_config();
    init_hmap(g_conf.hmap_size);
    tag_main_chunks();
    destroy_hmap();
}
