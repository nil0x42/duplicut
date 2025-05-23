#include <stdlib.h>
#include "file.h"
#include "hmap.h"
#include "hash.h"
#include "line.h"
#include "const.h"
#include "config.h"
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


/** Populate `hmap` with given `chunk` lines.
 * - Handle collisions & disable duplicate lines from `chunk`
 *   by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
void        populate_hmap(t_chunk *chunk)
{
    DLOG2("CALL populate_hmap()");
    char    *line_ptr;
    int     line_sz;
    long    slot;
    size_t  has_slots;
    char    *base_ptr;
    int     i;
    long    duplicates;
    const int has_dupfile = (g_conf.dupfile_name != NULL);

    duplicates = 0;
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

    size_t junk_lines = 0;
    while (get_next_line(&line_ptr, &line_sz, chunk, &junk_lines))
    {
        slot = HASH(line_ptr, line_sz) % g_hmap.size;
        has_slots = (g_hmap.size * 7) / 10;
        while (has_slots--)
        {
            if (!LINE_ISSET(g_hmap.ptr[slot]))
            {
                /* use first free slot */
                g_hmap.ptr[slot] = MAKE_LINE(line_ptr, line_sz);
                /* DLOG4("ADD HMAP ENTRY, slot=%ld, line_addr=%p, line_size=%d, line=%.*s", slot, LINE_ADDR(line), LINE_SIZE(line), LINE_SIZE(line), LINE_ADDR(line)); */
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
            else if (cmp_line(g_hmap.ptr[slot], line_ptr, line_sz) == 0)
            {
                if (has_dupfile) {
                    log_duplicate(line_ptr, line_sz);
                }
                *line_ptr = DISABLED_LINE;
                ++duplicates;
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
    set_status(TAGDUP_DUPLICATES, (size_t)duplicates);
    set_status(TAGDUP_JUNK_LINES, junk_lines);
#if DEBUG >= 2
    DLOG2("populate_hmap(): used %ld/%ld slots (%.2f%%) [%ld collisions]",
            filled, g_hmap.size,
            (double)filled / (double)g_hmap.size * 100.0,
            collisions);
#endif
}
