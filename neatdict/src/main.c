#include <unistd.h>
#include <stdio.h>
#include "config.h"
#include "chunk.h"
#include "line.h"
#include "debug.h"

t_chunk     *g_chunks = NULL;

t_conf      g_conf = {
    .memlimit = 0,
    .tmpdir = NULL,
    .threads = 0,
    .page_size = 0,
    .hmap_size = 0,
    .chunk_size = 0
};


/* function shall be removed */
void         display_chunk_infos(t_chunk *chunk)
{
    DLOG("------------------------------");
    DLOG("chunk->fd:            '%d'", chunk->fd);
    DLOG("chunk->tag:           '%d'", chunk->tag);
    DLOG("chunk->name:          '%s'", chunk->name);
    DLOG("chunk->addr:          '%p'", chunk->addr);
    DLOG("chunk->size:          '%ld'", chunk->size);
    DLOG("");
    DLOG("chunk->parent.fd:     '%d'", chunk->parent.fd);
    DLOG("chunk->parent.name:   '%s'", chunk->parent.name);
    DLOG("chunk->parent.offset: '%ld'", chunk->parent.offset);
    DLOG("chunk->parent.size:   '%ld'", chunk->parent.size);
    DLOG("");
    DLOG("chunk->map.addr:      '%p'", chunk->map.addr);
    DLOG("chunk->map.size:      '%ld'", chunk->map.size);
    DLOG("");
    DLOG("chunk->next:          '%p'", chunk->next);
    DLOG("------------------------------");
}

/* function shall be removed */
void        output_chunk(t_chunk *chunk)
{
    t_line  line;
    size_t  offset;

    offset = 0;
    while (next_line(&line, chunk, &offset) != NULL)
    {
        write(1, line.addr, line.size);
        write(1, "\n", 1);
    }
    /* write(1, "===SEP===\n", 10); */
}

int         main(int argc, char **argv)
{
    int     i;
    t_chunk *chunk;

    optparse(argc, argv, &i);
    configure();
    while (i < argc)
    {
        DLOG("--> argv[%d]: '%s'", i, argv[i]);
        chunkify_file(argv[i]);
        i++;
    }
    chunk = g_chunks;
    while (chunk != NULL)
    {
        display_chunk_infos(chunk);
        output_chunk(chunk);
        chunk = chunk->next;
    }
    return (0);
}
