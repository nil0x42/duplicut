#include <unistd.h>
#include <stdio.h>
#include "definitions.h"
#include "debug.h"

t_chunk         *g_chunks = NULL;

void            print_chunks(t_chunk *chunk)
{
    while (chunk != NULL)
    {
        printf("\n");
        printf("==========================\n");
        printf("fd   = '%d'\n",         chunk->fd);
        printf("name = '%s'\n",        chunk->name);
        printf("ptr  = '%p'\n",         chunk->ptr);
        printf("size = '%ld'\n",        chunk->size);
        printf("next = '%p'\n",         chunk->next);
        printf("-------------------------'\n");
        printf("file.fd     = '%d'\n",   chunk->file.fd);
        printf("file.name   = '%s'\n",   chunk->file.name);
        printf("file.offset = '%ld'\n",  chunk->file.offset);
        printf("file.size   = '%ld'\n",  chunk->file.size);
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
        DLOG("chunkify_file(argv[%d])", i);
        chunkify_file(argv[i]);
    }
    print_chunks(chunklist);
    return (EXIT_SUCCESS);
}
