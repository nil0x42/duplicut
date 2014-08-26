#include <unistd.h>
#include <stdio.h>
#include "definitions.h"
#include "debug.h"
#include "chunk.h"

int             main(int argc, char **argv)
{
    t_chunk     *chunklist;
    int         i;

    abort();
    if (argc != 2)
    {
        dprintf(STDERR_FILENO, "Usage: %s <wordlist>\n", argv[0]);
        return (EXIT_FAILURE);
    }
    i = 0;
    while (++i != argc)
    {
        /* if (memory_map(argv[1]) < 0) */
        /*     return (EXIT_FAILURE); */
        if (get_chunks(argv[i], &chunklist) < 0)
            return (EXIT_FAILURE);
    }
    /* DLOG("got all chunks"); */
    return (EXIT_SUCCESS);
}
