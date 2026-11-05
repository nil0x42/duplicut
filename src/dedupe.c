#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dedupe.h"
#include "const.h"
#include "config.h"
#include "thpool.h"
#include "file.h"
#include "chunk.h"
#include "hmap.h"
#include "status.h"
#include "error.h"
#include "debug.h"


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
void            tag_duplicates(void)
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
void            tag_duplicates(void)
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
