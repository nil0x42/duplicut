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

    if (chunk->endptr == file->addr + file->info.st_size)
    {
        memset(chunk, 0, sizeof(t_chunk));
        return (false);
    }

    // IS FIRST
    if (chunk->ptr == NULL)
    {
        ptr = file->addr;
        remaining_size = file->info.st_size;
    }
    // IS NOT FIRST
    else
    {
        ptr = chunk->endptr;
        remaining_size = file->info.st_size - (ptr - file->addr);
    }

    // IS LAST
    if (remaining_size <= (ssize_t) g_conf.chunk_size)
    {
        endptr = ptr + remaining_size;
        remaining_size = 0;
    }
    // IS NOT LAST
    else
    {
        endptr = ptr + g_conf.chunk_size;
        remaining_size -= g_conf.chunk_size;
        if ((endptr = memchr(endptr, '\n', remaining_size)) == NULL)
        {
            endptr = file->addr + file->info.st_size;
            remaining_size = 0;
        }
        else
        {
            endptr += 1; // set endptr AFTER the \n
            remaining_size -= endptr - (ptr + g_conf.chunk_size);
        }
    }

    chunk->ptr = ptr;
    chunk->endptr = endptr;
    DLOG4("get_next_chunk() returns ptr=%p, endptr=%p, size=%zd, remaining_size=%zd",
            chunk->ptr,
            chunk->endptr,
            chunk->endptr - chunk->ptr,
            remaining_size
         );
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
    long    duplicates;

    duplicates = 0;
    i = 0;
    base_ptr = chunk->ptr;
    size_t junk_lines = 0;
    while (get_next_line(&line, chunk, &junk_lines))
    {
        slot = HASH(&line) % g_hmap.size;
        while (LINE_ISSET(g_hmap.ptr[slot]))
        {
            if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                ++duplicates;
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
    set_status(TAGDUP_DUPLICATES, (size_t)duplicates);
    set_status(TAGDUP_JUNK_LINES, junk_lines);
    free(chunk);
    update_status(CTASK_DONE);
}
