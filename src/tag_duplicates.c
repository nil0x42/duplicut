#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tag_duplicates.h"
#include "const.h"
#include "config.h"
#include "thpool.h"
#include "file.h"
#include "chunk.h"
#include "hmap.h"
#include "error.h"
#include "debug.h"

#if MULTITHREADING == 1
/** For each chunk following `parent`, add a cleanout_chunk() worker.
 */
static void     tag_subchunks(threadpool thpool, const t_chunk *parent)
{
    t_chunk     chunk;
    t_chunk     *heap_chunk;
    int         chunk_id = 1;

    memcpy(&chunk, parent, sizeof(t_chunk));
    while (get_next_chunk(&chunk, g_file))
    {
        heap_chunk = malloc(sizeof(t_chunk));
        if (heap_chunk == NULL)
            die("could not malloc() heap_chunk");
        memcpy(heap_chunk, &chunk, sizeof(t_chunk));
        thpool_add_work(thpool, (void*)cleanout_chunk, heap_chunk);
        chunk_id ++;
    }
}

/** For each chunk, load it into hmap and remove all duplicates
 * in following chunks through `tag_subchunks()`.
 */
void            tag_duplicates(void)
{
    threadpool  thpool = thpool_init(g_conf.threads);
    t_chunk     main_chunk = {
        .ptr = NULL,
        .endptr = NULL
    };

    while (get_next_chunk(&main_chunk, g_file))
    {
        populate_hmap(&main_chunk);
        tag_subchunks(thpool, &main_chunk);
        thpool_wait(thpool);
    }

    thpool_destroy(thpool);
}


#else
/** Cleanout each chunk following the parent
 */
static void     tag_subchunks(const t_chunk *parent)
{
    t_chunk     chunk;
    t_chunk     *heap_chunk;
    int         chunk_id = 1;

    memcpy(&chunk, parent, sizeof(t_chunk));
    while (get_next_chunk(&chunk, g_file))
    {
        heap_chunk = malloc(sizeof(t_chunk));
        if (heap_chunk == NULL)
            die("could not malloc() heap_chunk");
        memcpy(heap_chunk, &chunk, sizeof(t_chunk));
        cleanout_chunk(heap_chunk);
        chunk_id ++;
    }
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
    int         chunk_id = 1;

    while (get_next_chunk(&main_chunk, g_file))
    {
        // TODO: afficher pourcentage d evolution (chunk 50 sur 234) -> 2%
        populate_hmap(&main_chunk);
        tag_subchunks(&main_chunk);
        chunk_id ++;
    }
}
#endif
