#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "chunk.h"
#include "line.h"
#include "file.h"
#include "hmap.h"
#include "hash.h"
#include "const.h"
#include "config.h"
#include "status.h"
#include "debug.h"


int         count_chunks(void)
{
    t_chunk     chunk = {
        .ptr = NULL,
        .endptr = NULL
    };
    int         result = 0;

    while (get_next_chunk(&chunk, g_file))
        result++;
    return (result);
}


/** Write next chunk from `file` into `chunk`
 * If `chunk->ptr` is NULL, chunk is set to `file`'s first chunk.
 * Otherwise, `chunk` is replaced by the next one.
 * If there is no next chunk, the function returns false.
 */
bool        get_next_chunk(t_chunk *chunk, struct file *file)
{
    char    *ptr;
    char    *endptr;
    ssize_t remaining_size;

    if (chunk->ptr == NULL)
    {
        ptr = file->addr;
        remaining_size = file->info.st_size;
    }
    else
    {
        ptr = chunk->endptr;
        remaining_size = file->info.st_size - (ptr - file->addr);
    }

    if (remaining_size <= 0)
    {
        memset(chunk, 0, sizeof(t_chunk));
        return (false);
    }

    if (remaining_size <= (ssize_t) g_conf.chunk_size)
        endptr = ptr + remaining_size;
    else
        endptr = ptr + g_conf.chunk_size;

    chunk->ptr = ptr;
    chunk->endptr = endptr;
    return (true);
}


/** Disable lines on `chunk` that already exist in `hmap`
 * by tagging them with 'DISABLED_LINE'
 * Main work for threads in thread pool.
 */
void        cleanout_chunk(t_chunk *chunk)
{
    t_line  line;
    long    slot;
    char    *base_ptr;
    int     i;

    i = 0;
    base_ptr = chunk->ptr;
    while (get_next_line(&line, chunk))
    {
        slot = HASH(&line) % g_hmap.size;
        while (LINE_ISSET(g_hmap.ptr[slot]))
        {
            if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            /* archaic open addressing collision resolver */
            slot = (slot + 1) % g_hmap.size;
        }
        i++;
        if (i == 500000) {
            set_status(TAGDUP_BYTES, (size_t)(chunk->ptr - base_ptr));
            base_ptr = chunk->ptr;
            i = 0;
        }
    }
    set_status(TAGDUP_BYTES, (size_t)(chunk->ptr - base_ptr));
    free(chunk);
    update_status(CTASK_DONE);
}
