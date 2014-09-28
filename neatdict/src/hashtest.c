#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "chunk.h"
#include "line.h"
#include "exit.h"
#include "debug.h"

#include "murmur3.h"
#include "fasthash.h"
#include "xxhash.h"


# define SEED       (0)


long            fasthash(t_line *line, long *out)
{
    uint64_t    h;

    h = fasthash64(line->addr, line->size, SEED);
    *out = (long)(h % g_conf.hmap_size);
}


long            xxhash(t_line *line, long *out)
{
    unsigned long long  h;

    h = XXH64(line->addr, line->size, SEED);
    *out = (long)(h % g_conf.hmap_size);
}


long            murmur3(t_line *line, long *out)
{
    uint32_t            buf128[4];
    unsigned long long  h;

    MurmurHash3_x64_128(line->addr, line->size, SEED, buf128);
    h = *((unsigned long long*)buf128);
    *out = (long)(h % g_conf.hmap_size);
}


void            populate_hmap(t_line *hmap, t_chunk *chunk)
{
    t_line      line;
    size_t      offset;
    long        hmap_size;
    long        h;

    long        iterations;
    long        collisions;

    offset = 0;
    iterations = 0;
    collisions = 0;
    while (next_line(&line, chunk, &offset) != NULL)
    {
        fasthash(&line, &h);
        /* DLOG("%lu\t%lu", h, g_conf.hmap_size); */
        if (hmap[h].size != 0)
            collisions++;
        else
            hmap[h] = line;
        iterations++;
    }

    double tmp;
    tmp = (double)1 / ((double)g_conf.hmap_size / (double)iterations);
    DLOG("loadfactor: %.1lf%%", tmp * 100);
    DLOG("iterations: %ld", iterations);
    tmp = (double)1 / ((double)iterations / (double)collisions);
    DLOG("collisions: %ld (%.1lf%%)", collisions, tmp * 100);
    DLOG("");
}

t_line          *create_hmap(void)
{
    t_line      *hmap;

    hmap = (t_line*) malloc(g_conf.hmap_size * sizeof(t_line));
    if (hmap == NULL)
        error("could not allocate hmap");
    return (hmap);
}

void            hashtest(void)
{
    t_line      *hmap;
    t_chunk     *chunk;

    hmap = create_hmap();
    chunk = g_chunks;
    while (chunk != NULL)
    {
        memset(hmap, 0, g_conf.hmap_size * sizeof(t_line));
        populate_hmap(hmap, chunk);
        chunk = chunk->next;
    }

}
