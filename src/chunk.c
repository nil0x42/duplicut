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


/** Fill `chunk` with next chunk from `file`.
 * If `chunk->ptr` is NULL, chunk is then initialized to first file chunk.
 * Otherwise, `chunk` is replaced by next one.
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
        remaining_size = ptr - file->addr - file->info.st_size;
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


/** zero mark lines in chunk if they exist in hmap
 * Main work for threads in pool.
 */
void        cleanout_chunk(t_chunk *chunk)
{
    t_line      line;
    long        slot;

    while (get_next_line(&line, chunk))
    {
        slot = hash(&line) % g_hmap.size;
        while (LINE_ISSET(g_hmap.ptr[slot]))
        {
            if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                /* apply zero tag */
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            /* archaic open addressing */
            slot = (slot + 1) % g_hmap.size;
        }
    }
    free(chunk);
}
