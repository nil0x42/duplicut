#include <stdlib.h>
#include "config.h"
#include "chunk.h"
#include "line.h"
#include "hash.h"
#include "exit.h"
#include "vars.h"
#include "debug.h"


/** Populate `hmap` whith given `chunk` lines.
 * - This function handles hash collisions and disables
 * duplicate lines from `chunk` by tagging them with 'DISABLED_LINE'.
 * - Unique lines are normally written into `hmap`.
 */
static void         populate_hmap(t_line *hmap, t_chunk *chunk)
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
            slot = (slot + 1) % g_conf.hmap_size;
        }
    }
}


/** Disable any line from `chunk` which is already present in `hmap`
 */
static void         cleanout_chunk(t_chunk *chunk, t_line *hmap)
{
    t_line          line;
    size_t          offset;
    long            slot;

    offset = 0;
    while (next_line(&line, chunk, &offset) != NULL)
    {
        slot = hash(&line);
        while (1)
        {
            if (LINE_SIZE(hmap[slot]) == 0)
                break;
            else if (cmp_line(&line, &hmap[slot]) == 0)
            {
                LINE_ADDR(line)[0] = DISABLED_LINE;
                break;
            }
            slot = (slot + 1) % g_conf.hmap_size;
        }
    }
}


/** Disable all duplicate lines, taking care of the order
 * between all chunks present in `main_chunk` linked list.
 */
void                remove_duplicates(t_chunk *main_chunk)
{
    t_chunk         *sub_chunk;

    g_vars.hmap = (t_line*) malloc(g_conf.hmap_size * sizeof(t_line));
    if (g_vars.hmap == NULL)
        error("could not allocate hmap");
    print_remaining_time();
    while (main_chunk != NULL)
    {
        populate_hmap(g_vars.hmap, main_chunk);
        g_vars.treated_chunks++;
        print_remaining_time();
        sub_chunk = main_chunk->next;
        while (sub_chunk != NULL)
        {
            cleanout_chunk(sub_chunk, g_vars.hmap);
            g_vars.treated_chunks++;
            print_remaining_time();
            sub_chunk = sub_chunk->next;
        }
        main_chunk = main_chunk->next;
    }
}
