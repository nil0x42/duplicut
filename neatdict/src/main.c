#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "definitions.h"
#include "config.h"
#include "vars.h"
#include "chunk.h"
#include "line.h"
#include "exit.h"
#include "debug.h"


t_conf      g_conf = {
    .memlimit = 0,
    .threads = 0,
    .line_max_size = DEFAULT_LINE_MAX_SIZE,
    .page_size = 0,
    .hmap_size = 0,
    .chunk_size = 0
};


t_vars      g_vars = {
    .chunk_list = NULL,
    .num_chunks = 0,
    .treated_chunks = 0,
    .hmap = NULL
};

int             main(int argc, char **argv)
{
    int         i;

    optparse(argc, argv, &i);
    configure();
    while (i < argc)
        chunkify_file(argv[i++], &g_vars.chunk_list);
    printf("\n");
    remove_duplicates(g_vars.chunk_list);
    exit_properly(0);
}
