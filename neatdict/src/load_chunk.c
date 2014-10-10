#include <sys/mman.h>
#include "chunk.h"
#include "config.h"
#include "exit.h"
/* #include "debug.h" */


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
    if (chunk->size == 0)
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
