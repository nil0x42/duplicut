#include <unistd.h>
#include <stdio.h>
#include "definitions.h"
#include "debug.h"

void            print_chunks(t_chunk *chunk)
{
    while (chunk != NULL)
    {
        printf("\n");
        printf("==========================\n");
        printf("ptr  = '%p'\n",         chunk->ptr);
        printf("size = '%ld'\n",        chunk->size);
        printf("next = '%p'\n",         chunk->next);
        printf("-------------------------'\n");
        printf("file.name  = '%s'\n",   chunk->file.name);
        printf("file.size  = '%ld'\n",  chunk->file.size);
        printf("file.fd    = '%d'\n",   chunk->file.fd);
        printf("--------------------------\n");
        printf("map.ptr    = '%p'\n",   chunk->map.ptr);
        printf("map.size   = '%ld'\n",  chunk->map.size);
        printf("map.offset = '%ld'\n",  chunk->map.offset);
        chunk = chunk->next;
    }
}

int             main(int argc, char **argv)
{
    t_chunk     *chunklist;
    int         i;

    chunklist = NULL;
    if (argc < 2)
    {
        dprintf(STDERR_FILENO, "Usage: %s [<wordlist>] ...\n", argv[0]);
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
    print_chunks(chunklist);
    /* DLOG("got all chunks"); */
    return (EXIT_SUCCESS);
}
