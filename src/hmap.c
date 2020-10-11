#include <stdlib.h>
#include "hmap.h"
#include "hash.h"
#include "const.h"
#include "status.h"
#include "error.h"
#include "debug.h"


void        init_hmap(size_t size)
{
    DLOG2("CALL init_hmap()");
    t_line  *area;

    if (g_hmap.ptr != NULL)
        error("init_hmap(): singleton called twice !");

    area = (t_line*) malloc(size * sizeof(t_line));
    if (area == NULL)
        die("cannot malloc() hash map");

    g_hmap.ptr = area;
    g_hmap.size = size;

    atexit(destroy_hmap);
}


void        destroy_hmap(void)
{
    DLOG2("CALL destroy_hmap()");
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
    DLOG2("CALL populate_hmap()");
    t_line  line;
    long    slot;
    size_t  has_slots;
    char    *base_ptr;
    int     i;

    i = 0;
    base_ptr = chunk->ptr;

#if DEBUG >= 2
    size_t      filled = 0;
    size_t      collisions = 0;
#endif
#if DEBUG >= 3
    int         last_percent_filled = 0;
    int         tmp = 0;
#endif

    memset(g_hmap.ptr, 0, g_hmap.size * sizeof(t_line));

    while (get_next_line(&line, chunk))
    {
        slot = HASH(&line) % g_hmap.size;
        has_slots = (g_hmap.size * 7) / 10;
        while (has_slots--)
        {
            if (!LINE_ISSET(g_hmap.ptr[slot]))
            {
                /* use first free slot */
                g_hmap.ptr[slot] = line;
#if DEBUG >= 2
                filled++;
#endif
#if DEBUG >= 3
                tmp = (int)((double)filled / (double)g_hmap.size * 100.0);
                if (tmp > last_percent_filled) {
                    last_percent_filled = tmp;
                    DLOG3("populate_hmap(): used %ld/%ld slots (%d%%) ...",
                            filled, g_hmap.size, tmp);
                }
#endif
                break;
            }
            else if (cmp_line(&line, &g_hmap.ptr[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            /* archaic open addressing collision resolver */
#if DEBUG >= 2
            ++collisions;
#endif
            slot = (slot + 1) % g_hmap.size;
        }
        if (!has_slots)
            error("populate_hmap(): too much slots in use");
        i++;
        if (i == 500000) {
            set_status(TAGDUP_BYTES, (size_t)(chunk->ptr - base_ptr));
            base_ptr = chunk->ptr;
            i = 0;
        }
    }
    set_status(TAGDUP_BYTES, (size_t)(chunk->ptr - base_ptr));
#if DEBUG >= 2
    DLOG2("populate_hmap(): used %ld/%ld slots (%.2f%%) [%ld collisions]",
            filled, g_hmap.size,
            (double)filled / (double)g_hmap.size * 100.0,
            collisions);
#endif
}
