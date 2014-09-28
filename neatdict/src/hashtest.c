#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "chunk.h"
#include "line.h"
#include "exit.h"
#include "debug.h"
#include "hash.h"


/** Populate `hmap` whith given `chunk` lines.
 * - This function handles hash collisions and disables
 * duplicate lines from `chunk` by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
void                populate_hmap(t_line *hmap, t_chunk *chunk)
{
    t_line          line;
    size_t          offset;
    long            slot;

    memset(hmap, 0, g_conf.hmap_size * sizeof(t_line));
    offset = 0;
    while (next_line(&line, chunk, &offset) != NULL)
    {
        slot = hash(&line);
        while (1)
        {
            if (LINE_SIZE(hmap[slot]) == 0)
            {
                hmap[slot] = line;
                break;
            }
            else if (cmp_line(&line, &hmap[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            else
                slot = (slot + 1) % g_conf.hmap_size;
        }
    }
}


void            hashtest(void)
{
    t_line      *hmap;
    t_chunk     *chunk;

    hmap = (t_line*) malloc(g_conf.hmap_size * sizeof(t_line));
    if (hmap == NULL)
        error("could not allocate hmap");
    chunk = g_chunks;
    while (chunk != NULL)
    {
        populate_hmap(hmap, chunk);
        chunk = chunk->next;
    }
}
