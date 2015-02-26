#include <stdlib.h>
#include "filehandle.h"
#include "config.h"
#include "thpool.h"
#include "chunk.h"
#include "error.h"
#include "hash.h"
#include "line.h"
#include "vars.h"
#include "definitions.h"


/** Populate `hmap` whith given `chunk` lines.
 * - This function handles hash collisions and disables
 * duplicate lines from `chunk` by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
static void         populate_hmap(t_chunk *chunk)
{
    t_line      *hmap;
    t_line      line;
    long        slot;

    hmap = g_vars.hmap;
    memset(hmap, 0, g_conf.hmap_size * sizeof(t_line));

    while (get_next_line(&line, chunk))
    {
        slot = hash(&line) % g_conf.hmap_size;
        while (1)
        {
            if (!LINE_ISSET(hmap[slot]))
            {
                /* use first free slot */
                hmap[slot] = line;
                break;
            }
            else if (cmp_line(&line, &hmap[slot]) == 0)
            {
                /* if line already in hmap, zero tag duplicate */
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            slot = (slot + 1) % g_conf.hmap_size;
        }
    }
}


/** zero mark lines in chunk if they exist in hmap
 * Main work for threads in pool.
 */
static void         cleanout_chunk(t_chunk *chunk)
{
    t_line      line;
    long        slot;

    while (get_next_line(&line, chunk))
    {
        slot = hash(&line) % g_conf.hmap_size;
        while (LINE_ISSET(g_vars.hmap[slot]))
        {
            if (cmp_line(&line, &g_vars.hmap[slot]) == 0)
            {
                /* apply zero tag */
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            /* archaic open addressing */
            slot = (slot + 1) % g_conf.hmap_size;
        }
    }
    free(chunk);
}


/** For each chunk following `parent`, add a cleanout_chunk() worker.
 */
static void         populate_thpool(threadpool thpool, const t_chunk *parent)
{
    t_chunk     chunk;

    memcpy(&chunk, parent, sizeof(t_chunk));
    while (get_next_chunk(&chunk, g_file))
    {
        t_chunk *worker_chunk = malloc(sizeof(t_chunk));
        if (worker_chunk == NULL)
            die("could not malloc() worker_chunk");
        thpool_add_work(thpool, (void*)cleanout_chunk, worker_chunk);
    }
}


/** For each chunk, load it into hmap and remove all duplicates
 * in following chunks through `populate_thpool()`.
 */
void                tag_duplicates(t_file *file)
{
    threadpool  thpool = thpool_init(g_conf.threads);
    t_chunk     main_chunk = {0};

    while (get_next_chunk(&main_chunk, g_file))
    {
        populate_hmap(&main_chunk);
        populate_thpool(thpool, &main_chunk);
        thpool_wait(thpool);
    }

    thpool_destroy(thpool);
}
