#include <stdlib.h>
#include "hmap.h"
#include "hash.h"
#include "const.h"
#include "error.h"
#include "debug.h"


void        init_hmap(size_t size)
{
    DLOG("init_hmap()");
    t_line  *area;

    if (g_hmap.ptr != NULL)
        error("create_hmap(): singleton called twice !");

    area = (t_line*) malloc(size * sizeof(t_line));
    if (area == NULL)
        error("cannot malloc() hash map: %s", ERRNO);

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
 * - Handle collisions & disable duplicate lines from `chunk`
 *   by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
void        populate_hmap(t_chunk *chunk)
{
    DLOG("populate_hmap()");
    t_line      line;
    long        slot;
    size_t      has_slots;

#ifdef DEBUG
    int         last_percent_filled = 0;
    int         tmp = 0;
    size_t      filled = 0;
#endif

    memset(g_hmap.ptr, 0, g_hmap.size * sizeof(t_line));

    while (get_next_line(&line, chunk))
    {
        slot = hash(&line) % g_hmap.size;
        has_slots = (g_hmap.size * 10) / 8;
        while (has_slots--)
        {
            if (!LINE_ISSET(g_hmap.ptr[slot]))
            {
                /* use first free slot */
                g_hmap.ptr[slot] = line;
#ifdef DEBUG
                filled++;
                /* tmp = (int)((double)filled / (double)g_hmap.size * 100.0); */
                /* if (tmp > last_percent_filled) { */
                /*     last_percent_filled = tmp; */
                /*     DLOG("populate_hmap(): used %ld/%ld slots (%d%%) ...", */
                /*             filled, g_hmap.size, tmp); */
                /* } */
#endif
                break;
            }
            else if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            /* archaic open addressing collision resolver */
            slot = (slot + 1) % g_hmap.size;
        }
        if (!has_slots)
            error("populate_hmap(): no space left on hashmap.");
    }
#ifdef DEBUG
    DLOG("populate_hmap(): used %ld/%ld slots (%.2f%%)",
            filled, g_hmap.size, (double)filled / (double)g_hmap.size * 100.0);
#endif
}
