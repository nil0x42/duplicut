#include <stdlib.h>
#include <string.h>
#include "optparse.h"
#include "config.h"
#include "thpool.h"
#include "file.h"
#include "chunk.h"
#include "hmap.h"
#include "error.h"
#include "debug.h"


struct conf g_conf = {
    .infile_name = NULL,
    .outfile_name = NULL,
    .threads = 0,
    .line_max_size = DEFAULT_LINE_MAX_SIZE,
    .hmap_size = 0,
    .chunk_size = 0
};

struct file *g_file;
struct hmap g_hmap;


/** For each chunk following `parent`, add a cleanout_chunk() worker.
 */
static void     populate_thpool(threadpool thpool, const t_chunk *parent)
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
static void     tag_duplicates(void)
{
    threadpool  thpool = thpool_init(g_conf.threads);
    t_chunk     main_chunk = {
        .ptr = NULL,
        .endptr = NULL
    };

    DLOG("tag_duplicates()");

    while (get_next_chunk(&main_chunk, g_file))
    {
        DLOG("  main_chunk = %p / %p", main_chunk.ptr, main_chunk.endptr);
        populate_hmap(&main_chunk);
        populate_thpool(thpool, &main_chunk);
        thpool_wait(thpool);
    }

    thpool_destroy(thpool);
}


int             main(int argc, char **argv)
{
    optparse(argc, argv); /* set g_conf options */

    init_file(g_conf.infile_name, g_conf.outfile_name);
    config(); /* configure g_conf options */

    init_hmap(g_conf.hmap_size);
    tag_duplicates();
    destroy_hmap();

    /* remove_duplicates(g_vars.chunk_list); */
    destroy_file();
    return (0);
}
