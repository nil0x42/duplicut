#include <fcntl.h>
#include "definitions.h"
#include "error.h"


int         main(int argc, char **argv)
{
    if (argc != 2)
    {
        dprintf(STDERR_FILENO, "Usage: %s <wordlist>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    if (memory_map(argv[1]) < 0)
        return (EXIT_FAILURE);
    close(-1);
    return (EXIT_SUCCESS);
}
