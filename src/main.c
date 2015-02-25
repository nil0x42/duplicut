#include <stdlib.h>
#include <stdio.h>
#include "optparse.h"
#include "config.h"
#include "vars.h"
#include "hmap.h"


struct conf g_conf = {
    .infile_name = NULL,
    .outfile_name = NULL,
    .threads = 0,
    .line_max_size = DEFAULT_LINE_MAX_SIZE,
    .hmap_size = 0,
    .chunk_size = 0
};


t_vars      g_vars = {
    .chunk_list = NULL,
    .num_chunks = 0,
    .treated_chunks = 0,
    .hmap = NULL
};


int         main(int argc, char **argv)
{
    int     dstfile_fd;

    optparse(argc, argv);
    configure();
    dstfile_fd = filehandle(g_conf.infile_name, g_conf.outfile_name);


    atexit(delete_hmap);
    tag_duplicates(g_vars.chunk_list);
    remove_duplicates(g_vars.chunk_list);
    return (0);
}
