#include <stdlib.h>
#include "hmap.h"
#include "hash.h"
#include "const.h"
#include "error.h"


void        init_hmap(size_t size)
{
    t_line  *area;

    if (g_hmap.ptr != NULL)
        error("create_hmap(): singleton called twice !");

    area = (t_line*) malloc(size * sizeof(t_line));
    if (area == NULL)
    {
        error("cannot malloc() hash map: %s", ERRNO);
    }

    g_hmap.ptr = area;
    g_hmap.size = size;

    atexit(destroy_hmap);
}


void        destroy_hmap(void)
{
    if (g_hmap.ptr != NULL)
    {
        free(g_hmap.ptr);
        g_hmap.ptr = NULL;
    }
}


/** Populate `hmap` whith given `chunk` lines.
 * - This function handles hash collisions and disables
 * duplicate lines from `chunk` by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
void        populate_hmap(t_chunk *chunk)
{
    t_line      line;
    long        slot;

    memset(g_hmap.ptr, 0, g_hmap.size * sizeof(t_line));

    while (get_next_line(&line, chunk))
    {
        slot = hash(&line) % g_hmap.size;
        while (1)
        {
            if (!LINE_ISSET(g_hmap.ptr[slot]))
            {
                /* use first free slot */
                g_hmap.ptr[slot] = line;
                break;
            }
            else if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                /* if line already in hmap, zero tag duplicate */
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            slot = (slot + 1) % g_hmap.size;
        }
    }
}
