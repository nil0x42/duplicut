#include <stdlib.h>
#include <string.h>
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

    memcpy(&chunk, parent, sizeof(t_chunk));
    while (get_next_chunk(&chunk, g_file))
    {
        t_chunk *current_chunk = malloc(sizeof(t_chunk));
        if (current_chunk == NULL)
            die("could not malloc() current_chunk");
        thpool_add_work(thpool, (void*)cleanout_chunk, current_chunk);
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
        DLOG("  main_chunk = %p / %p", main_chunk.ptr, main_chunk.endptr);
        populate_hmap(&main_chunk);
        tag_subchunks(thpool, &main_chunk);
        thpool_wait(thpool);
    }

    thpool_destroy(thpool);
}


#else
/** For each chunk following `parent`, add a cleanout_chunk() worker.
 */
static void     tag_subchunks(const t_chunk *parent)
{
    t_chunk     chunk;

    memcpy(&chunk, parent, sizeof(t_chunk));
    while (get_next_chunk(&chunk, g_file))
    {
        t_chunk *current_chunk = malloc(sizeof(t_chunk));
        if (current_chunk == NULL)
            die("could not malloc() current_chunk");
        cleanout_chunk(current_chunk);
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

    while (get_next_chunk(&main_chunk, g_file))
    {
        DLOG("  main_chunk = %p / %p", main_chunk.ptr, main_chunk.endptr);
        populate_hmap(&main_chunk);
        tag_subchunks(&main_chunk);
    }
}
#endif
