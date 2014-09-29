#include <unistd.h>
#include <stdio.h>
#include "config.h"
#include "vars.h"
#include "chunk.h"
#include "line.h"
#include "exit.h"
#include "debug.h"


t_conf      g_conf = {
    .memlimit = 0,
    .tmpdir = NULL,
    .threads = 0,
    .page_size = 0,
    .hmap_size = 0,
    .chunk_size = 0
};


t_vars      g_vars = {
    .chunk_list = NULL,
    .num_chunks = 0,
    .treated_chunks = 0
};


void         display_chunk_infos(t_chunk *chunk)
{
    DLOG("------------------------------");
    DLOG("chunk->fd:            '%d'",  chunk->fd);
    DLOG("chunk->tag:           '%d'",  chunk->tag);
    DLOG("chunk->name:          '%s'",  chunk->name);
    DLOG("chunk->addr:          '%p'",  chunk->addr);
    DLOG("chunk->size:          '%ld'", chunk->size);
    DLOG("");
    DLOG("chunk->parent.fd:     '%d'",  chunk->parent.fd);
    DLOG("chunk->parent.name:   '%s'",  chunk->parent.name);
    DLOG("chunk->parent.offset: '%ld'", chunk->parent.offset);
    DLOG("chunk->parent.size:   '%ld'", chunk->parent.size);
    DLOG("");
    DLOG("chunk->map.addr:      '%p'",  chunk->map.addr);
    DLOG("chunk->map.size:      '%ld'", chunk->map.size);
    DLOG("");
    DLOG("chunk->next:          '%p'",  chunk->next);
    DLOG("------------------------------");
}



int         main(int argc, char **argv)
{
    int     i;
    t_line  *hmap;
    t_chunk *chunk;

    optparse(argc, argv, &i);
    configure();
    while (i < argc)
        chunkify_file(argv[i++], &g_vars.chunk_list);
    printf("\n");
    remove_duplicates(g_vars.chunk_list);
    exit_properly(0);
}
