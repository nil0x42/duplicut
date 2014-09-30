#include <unistd.h>
#include <string.h>
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
    .treated_chunks = 0,
    .hmap = NULL
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


/* #define SUFFIX   (".neat") */
/* #define BUF_SIZE (CHUNK_FILE + 5) */
/* void            blabla(t_chunk *chunk) */
/* { */
/*     char        *src_file; */
/*     char        dst_file[CHUNK_PATHSIZE + 5]; */
/*     char        *dst_ptr; */
/*     size_t      *dst_size; */
/*     int         dst_fd; */
/*     size_t      dst_offset; */
/*  */
/*     do */
/*     { */
/*         src_file = chunk->name; */
/*         memset(dst_file, 0, BUF_SIZE); */
/*         strcpy(dst_file, BUF_SIZE); */
/*         strcat(dst_file, SUFFIX); */
/*         dst_fd = open(dst_file, O_CREAT | O_WRONLY); */
/*         dst_size = chunk->parent_size + 1; */
/*         dst_offset = 0; */
/*         ftruncate(fd, chunk->parent->size + 1); */
/*         dst_ptr = mmap(NULL, dst_size, PROT_WRITE, MAP_SHARED, */
/*                        dst_fd, dst_offset); */
/*         while (chunk != NULL && chunk->name == src_file) */
/*         { */
/*             dstfile_writeln(); */
/*             chunk = chunk->next; */
/*         } */
/*     } */
/*     while (chunk != NULL); */
/* } */


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
