#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "definitions.h"
#include "exit.h"

int                 memory_map(const char *pathname)
{
    void            *ptr;
    struct stat     info;
    int             fd;

    if ((fd = open(pathname, O_RDONLY)) < 0)
        error("cannot open %s: %s", pathname, ERRNO);
    else if (fstat(fd, &info) < 0)
        error("cannot stat %s: %s", pathname, ERRNO);
    else if (!S_ISREG(info.st_mode))
        error("%s: not a regular file", pathname);
    else
    {
        ptr = mmap(NULL, info.st_size + 9999, PROT_READ, MAP_PRIVATE, fd, 4096);
        if (ptr == MAP_FAILED)
            error("cannot map %s: %s", pathname, ERRNO);
        /* else if (*((char*)ptr + info.st_size - 1) != '\n') */
        /*     error("%s: should end with a newline ('\\n')", pathname); */
        else
            return (remove_duplicates(ptr, info.st_size));
    }

    close(fd);
    return (-1);
}
