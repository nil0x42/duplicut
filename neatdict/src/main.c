#include <stdio.h>
#include "definitions.h"

t_chunk     *g_chunks;


int         main(int argc, char **argv)
{
    int     i;
    /* void    *ptr; */

    configure(argc, argv, &i);
    while (i < argc)
    {
        DLOG("--> argv[%d]: '%s'", i, argv[i]);
        chunkify_file(argv[i]);
        i++;
    }
    t_chunk *chunk;
    chunk = g_chunks;
    while (chunk != NULL)
    {
        /* display_chunk_infos(chunk); */
        /* init_chunk(chunk); */
        /* display_chunk_infos(chunk); */
        output_chunk(chunk);

        chunk = chunk->next;
    }
    return (0);
}
