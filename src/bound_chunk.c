#define _GNU_SOURCE
#include <string.h>
#include "chunk.h"
#include "config.h"
#include "exit.h"


/** Get real chunk offset (start boundary).
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
static void         get_chunk_offset(t_chunk *chunk)
{
    char            *ptr;

    if (chunk->tag & FIRST_CHUNK)
    {
        chunk->offset = 0;
    }
    else if (chunk->map.addr[g_conf.page_size - 1] == '\n')
    {
        chunk->offset = g_conf.page_size;
    }
    else
    {
        ptr = memrchr(chunk->map.addr, '\n', g_conf.page_size);
        if (ptr == NULL)
        {
            chunk->offset = 0;
            return ;
            /* int fd = open("/tmp/xxx", O_TRUNC | O_WRONLY | O_CREAT, 0666); */
            /* if (fd < 0) */
            /*     error(ERRNO); */
            /* write(fd, chunk->map.addr, chunk->map.size); */
            /* error("%d@%s: can't find start boundary", */
            /*         chunk->id, chunk->file.name); */
        }
        ptr += 1;
        chunk->offset = (size_t)(ptr - chunk->map.addr);
    }
}


/** Get real chunk size (end boundary).
 */
static void         get_chunk_size(t_chunk *chunk)
{
    char            *chunk_addr;
    char            *ptr;

    chunk_addr = &chunk->map.addr[chunk->offset];
    chunk->size = chunk->map.size - chunk->offset;
    if (!(chunk->tag & LAST_CHUNK))
    {
        if (chunk_addr[chunk->size - 1] == '\n')
        {
            return ;
        }
        else
        {
            ptr = memrchr(chunk_addr, '\n', chunk->size - 1);
            if (ptr == NULL)
            {
                return ;
                /* error("%d@%s: can't find end boundary", */
                /*         chunk->id, chunk->file.name); */
            }
            ptr += 1;
            chunk->size -= chunk->size - (size_t)(ptr - chunk_addr);
        }
    }
}


/** Determine real bounaries for the given chunk.
 * (Because chunks are created from memory mappings, so
 * real bounaries shall be related to start/end of a
 * specific line of the wordlist).
 */
void                bound_chunk(t_chunk *chunk)
{
    get_chunk_offset(chunk);
    get_chunk_size(chunk);
}
