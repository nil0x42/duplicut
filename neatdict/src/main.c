#include <stdio.h>
#include "definitions.h"

t_chunk     *g_chunks;

int         main(int argc, char **argv)
{
    int     i;

    configure(argc, argv, &i);
    while (i < argc)
    {
        printf("%s\n", argv[i]);
        i++;
    }
    return (0);
}
