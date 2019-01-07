#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "optparse.h"
#include "config.h"
#include "file.h"
#include "hmap.h"
#include "tag_duplicates.h"
#include "user_input.h"
#include "status.h"
#include "error.h"


struct conf g_conf = {
    .infile_name = NULL,
    .outfile_name = NULL,
    .threads = 0,
    .line_max_size = DEFAULT_LINE_MAX_SIZE,
    .hmap_size = 0,
    .chunk_size = 0,
    .filter_printable = 0,
    .memlimit = LONG_MAX,
};

struct file *g_file;

struct hmap g_hmap;


/** Rewrite file, ignoring lines tagged with 'DISABLED_LINE'
 */
static void     remove_duplicates(void)
{
    t_chunk     file_chunk;
    t_line      line;
    size_t      line_size;
    char        *dst;

    file_chunk.ptr = g_file->addr;
    file_chunk.endptr = g_file->addr + g_file->info.st_size;

    dst = file_chunk.ptr;
    while (get_next_line(&line, &file_chunk))
    {
        line_size = LINE_SIZE(line);
        memmove(dst, LINE_ADDR(line), line_size);
        dst += line_size;
        if (dst != file_chunk.endptr)
            *dst++ = '\n';
    }

    /* update file size */
    g_file->info.st_size = dst - g_file->addr;
}


int             main(int argc, char **argv)
{
    optparse(argc, argv); /* setup g_conf options */

    if (isatty(STDIN_FILENO))
        watch_user_input();

    update_status(FCOPY_START);
    init_file(g_conf.infile_name, g_conf.outfile_name);
    config(); /* configure g_conf options */

    init_hmap(g_conf.hmap_size);
    update_status(TAGDUP_START);
    tag_duplicates();
    destroy_hmap();

    update_status(FCLEAN_START);
    remove_duplicates();
    destroy_file();
    return (0);
}
