#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "optparse.h"
#include "config.h"
#include "file.h"
#include "hmap.h"
#include "partition.h"
#include "dedupe.h"
#include "progressbar.h"
#include "status.h"
#include "error.h"
#include "debug.h"


struct conf g_conf = {
    .infile_name = NULL,
    .outfile_name = "/dev/stdout",
    .dupfile_name = NULL,
    .threads = 0,
    .line_max_size = DEFAULT_LINE_MAX_SIZE,
    .hmap_size = 0,
    .chunk_size = 0,
    .filter_printable = 0,
    .memlimit = LONG_MAX,
    .dedupe_memory = 0,
    .partition_count = 0,
    .partition_workers = 0,
    .mmap_window = 0,
    .page_size = 0,
    .engine = ENGINE_AUTO,
};

struct file *g_file;

struct hmap g_hmap;


/** Rewrite file, ignoring lines tagged with 'DISABLED_LINE'
 */
static void     remove_duplicates(void)
{
    t_chunk     file_chunk;
    t_chunk     range;
    t_chunk     mapped;
    char        *output_start;
    char        *line_ptr;
    int         line_sz;
    char        *dst;
    char        *base_ptr;
    int         i;

    file_chunk.ptr = g_file->addr;
    file_chunk.endptr = g_file->addr + g_file->info.st_size;

    i = 0;
    base_ptr = file_chunk.ptr;

    dst = file_chunk.ptr;
    size_t junk_lines = 0;
    range = file_chunk;
    while (range.ptr < range.endptr)
    {
        file_chunk = get_chunk_window(&range);
        mapped = file_chunk;
        output_start = dst;
        while (get_next_line(&line_ptr, &line_sz, &file_chunk, &junk_lines))
        {
            memmove(dst, line_ptr, line_sz);
            dst += line_sz;
            if (dst != range.endptr)
                *dst++ = '\n';
            i++;
            if (i == 500000) {
                set_status(FCLEAN_BYTES, (size_t)(file_chunk.ptr - base_ptr));
                base_ptr = file_chunk.ptr;
                i = 0;
            }
        }
        release_chunk(&mapped);
        release_chunk(&(t_chunk){output_start, dst});
        range.ptr = mapped.endptr;
    }

    /* Count filtered lines once, excluding the duplicates already reported. */
    set_status(FCLEAN_SKIPPED, junk_lines);

    /* update file size */
    g_file->info.st_size = dst - g_file->addr;
    set_status(FCLEAN_BYTES, (size_t)(range.endptr - base_ptr));
}


int             main(int argc, char **argv)
{
    optparse(argc, argv); /* setup g_conf options */

    start_progressbar();

    update_status(FCOPY_START);
    init_files();
    config(); /* configure g_conf options */
    set_status(CHUNK_SIZE, g_conf.chunk_size);

    update_status(TAGDUP_START);
    if (!g_conf.partition_count || !tag_partitions())
    {
        DLOG1("dedupe engine: mmap-chunks");
        update_status(CHUNK_START);
        init_hmap(g_conf.hmap_size);
        tag_duplicates();
        destroy_hmap();
    }

    update_status(FCLEAN_START);
    remove_duplicates();
    cleanup_files();

    stop_progressbar();

    display_report();
    return (0);
}
