#define _GNU_SOURCE
#include <string.h>
#include <sys/mman.h>
#include "chunk.h"
#include "config.h"
#include "exit.h"
#include "debug.h"


/** Determine real bounaries for the given chunk.
 * (Because chunks are created from memory mappings, so
 * real bounaries shall be related to start/end of a
 * specific line of the wordlist).
 */
static void         bound_chunk(t_chunk *chunk)
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
            error("%d@%s: can't find start boundary",
                    chunk->id, chunk->file.name);
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
                error("%d@%s: can't find end boundary",
                        chunk->id, chunk->file.name);
            ptr++;
            delta = (size_t)(ptr - chunk->addr);
            chunk->size -= chunk->size - delta;
        }
    }
}



void                load_chunk(t_chunk *chunk)
{
    /* dlog_obj_t_chunk(chunk); */
    chunk->map.addr = mmap(NULL, chunk->map.size, (PROT_READ | PROT_WRITE),
            MAP_SHARED, chunk->file.fd, chunk->file.offset);
    if (chunk->map.addr == MAP_FAILED)
        error("cannot mmap() chunk %d@%s: %s",
                chunk->id, chunk->file.name, ERRNO);
    /* if (mlock(chunk->map.addr, chunk->map.size) < 0) */
    /*     error("cannot mlock() chunk %d@%s: %s", */
    /*             chunk->id, chunk->file.name, ERRNO); */

    // bound chunk (only if needed)
    // // commented because addr changes after each load
    /* if (chunk->addr == NULL) */
    /*     bound_chunk(chunk); */
    bound_chunk(chunk);
}


void                unload_chunk(t_chunk *chunk)
{
    /* if (munlock(chunk->map.addr, chunk->map.size) < 0) */
    /*     error("cannot munlock() chunk %d@%s: %s", */
    /*             chunk->id, chunk->file.name, ERRNO); */
    if (munmap(chunk->map.addr, chunk->map.size) < 0)
        error("cannot munmap() chunk %d@%s: %s",
                chunk->id, chunk->file.name, ERRNO);
}
