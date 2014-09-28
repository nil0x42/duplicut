#define _GNU_SOURCE
#include <sys/mman.h>
#include "config.h"
#include "chunk.h"
#include "exit.h"
#include "debug.h"


static void         get_chunk_boundaries(t_chunk *chunk)
{
    char            *ptr;
    size_t          delta;

    if (chunk->tag & FIRST_CHUNK)
    {
        delta = 0;
        chunk->addr = chunk->map.addr;
    }
    else if (chunk->map.addr[g_conf.page_size - 1] == '\n')
    {
        delta = g_conf.page_size;
        chunk->addr = &chunk->map.addr[g_conf.page_size];
    }
    else
    {
        chunk->addr = memrchr(chunk->map.addr, '\n', g_conf.page_size);
        if (chunk->addr == NULL)
            error("%s: can't find start bound", chunk->name);
        chunk->addr++;
        delta = (size_t)(chunk->addr - chunk->map.addr);
    }
    chunk->size = chunk->map.size - delta;
    if (!(chunk->tag & LAST_CHUNK))
    {
        if (chunk->addr[chunk->size - 1] == '\n')
            return ;
        else
        {
            ptr = memrchr(chunk->addr, '\n', chunk->size - 1);
            if (ptr == NULL)
                error("%s: can't find end bound", chunk->name);
            ptr++;
            delta = (size_t)(ptr - chunk->addr);
            chunk->size -= chunk->size - delta;
        }
    }
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
