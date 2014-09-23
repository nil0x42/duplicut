#define _GNU_SOURCE
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "definitions.h"
#include "exit.h"

extern t_chunk      *g_chunks;
extern t_conf       g_conf;


/* #<{(| function shall be removed |)}># */
/* void        output_chunk(t_chunk *chunk) */
/* { */
/*     char    *ptr; */
/*     size_t  off; */
/*  */
/*     char    *eof; */
/*     char    *end; */
/*     int     len; */
/*     int     lines = 0; */
/*  */
/*     ptr = chunk->addr; */
/*     off = chunk->size; */
/*     eof = ptr + off; */
/*     while (ptr != eof) */
/*     { */
/*         end = memchr(ptr, '\n', off); */
/*         len = end - ptr; */
/*         if (len >= off) */
/*             break; */
/*         off -= len; */
/*         DLOG("%d", len); */
/*         DLOG("%ld", (size_t)off); */
/*         #<{(| write(1, ptr, len); |)}># */
/*         #<{(| write(1, "\n", 1); |)}># */
/*         ptr = end + 1; */
/*         ++lines; */
/*     } */
/*     exit(1); */
/*     #<{(| printf("\n\n\n\n\n\n\n\n\n"); |)}># */
/*     #<{(| DLOG(""); |)}># */
/*     #<{(| DLOG("found %d lines\n", lines); |)}># */
/* } */


void            output_chunk(t_chunk *chunk)
{
    t_line      *line;

    write(1, chunk->addr, chunk->size);
    /* write(1, "===SEP===\n", 10); */
}



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


/** Binds the given chunks to main chunk list (g_chunks).
 */
static void         attach_chunk(t_chunk *chunk)
{
    static t_chunk  *last_created_chunk = NULL;

    if (g_chunks == NULL)
        g_chunks = chunk;
    if (last_created_chunk != NULL)
        last_created_chunk->next = chunk;
    last_created_chunk = chunk;
}


static void         get_chunk_boundaries(t_chunk *chunk)
{
    char            *ptr;
    size_t          delta;

    DLOG("+++++++ CHUNK %s ++++++++", chunk->name);
    if (chunk->tag & FIRST_CHUNK)
    {
        DLOG("C1");
        delta = 0;
        chunk->addr = chunk->map.addr;
    }
    else if (chunk->map.addr[g_conf.page_size - 1] == '\n')
    {
        DLOG("C2");
        delta = g_conf.page_size;
        chunk->addr = &chunk->map.addr[g_conf.page_size];
    }
    else
    {
        DLOG("C3");
        chunk->addr = memrchr(chunk->map.addr, '\n', g_conf.page_size);
        if (chunk->addr == NULL)
            error("%s: can't find start bound", chunk->name);
        chunk->addr++;
        delta = (size_t)(chunk->addr - chunk->map.addr);
    }
    DLOG("DELTA1::%lu", delta);

    chunk->size = chunk->map.size - delta;

    if (!(chunk->tag & LAST_CHUNK))
    {
        if (chunk->addr[chunk->size - 1] == '\n')
        {
            DLOG("X2");
            return ;
        }
        else
        {
            DLOG("X3");
            ptr = memrchr(chunk->addr, '\n', chunk->size - 1);
            if (ptr == NULL)
                error("%s: can't find end bound", chunk->name);
            ptr++;
            delta = (size_t)(ptr - chunk->addr);
            DLOG("DELTA2::%lu", delta);
            chunk->size -= chunk->size - delta;
        }
    }
    else
        DLOG("X1");
}


/** Write chunk from parent file at given offset into chunk file
 */
void                init_chunk(t_chunk *chunk)
{
    void            *src;
    void            *dst;

    src = mmap(NULL, chunk->map.size, PROT_READ, MAP_SHARED,
            chunk->parent.fd, chunk->parent.offset);
    if (src == MAP_FAILED)
        error("%s: can't map file for reading at offset %ld: %s",
                chunk->parent.name, chunk->parent.offset, ERRNO);
    dst = mmap(NULL, chunk->map.size, PROT_WRITE, MAP_SHARED,
            chunk->fd, 0);
    if (dst == MAP_FAILED)
        error("%s: could not map chunk for writing: %s", chunk->name, ERRNO);
    memcpy(dst, src, chunk->map.size);
    chunk->map.addr = dst;
    if (munmap(src, chunk->map.size) < 0)
        error("%s: could not unmap file for reading at offset %ld",
                chunk->parent.name, chunk->parent.offset);
    get_chunk_boundaries(chunk);
    /* if (munmap(dst, chunk->map.size) < 0) */
    /*     error("%s: could not unmap chunk for writing", chunk->name); */
}


/** Create a new chunk based on a portion of file.
 * This function appends the new allocated chunk
 * to `g_chunks`. It is created by copying
 * `size` bytes of `file->fd`, starting at `file->offset`
 * offset to a temporary file.
 */
t_chunk             *create_chunk(t_file *file, size_t size)
{
    t_chunk         *chunk;

    if ((chunk = (t_chunk*) malloc(sizeof(t_chunk))) == NULL)
        return (NULL);
    memset(chunk, 0, sizeof(t_chunk));

    memcpy(&chunk->parent, file, sizeof(*file));
    chunk->map.size = size;
    strcpy(chunk->name, g_conf.tmpdir);
    strcat(chunk->name, CHUNK_FILENAME);
    if ((chunk->fd = mkstemps(chunk->name, 6)) < 0)
        error("cannot create chunk %s: %s", chunk->name, ERRNO);
    if (ftruncate(chunk->fd, chunk->map.size) < 0)
        error("cannot truncate chunk %s: %s", chunk->name, ERRNO);
    /* write_chunk(chunk); */
    attach_chunk(chunk);
    return (chunk);
}
