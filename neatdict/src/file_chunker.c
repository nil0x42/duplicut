#include <sys/stat.h>
#include "chunk.h"
#include "error.h"

t_chunk     *get_file_chunks(const char *pathname)
{
    int             fd;
    struct stat     info;
    void            *ptr;

    if ((fd = open(pathname, O_RDONLY)) < 0)
        error("cannot open %s: %s", pathname, ERRNO);
    else if (fstat(fd, &info) < 0)
        error("cannot stat %s: %s", pathname, ERRNO);
    else if (!S_ISREG(info.st_mode))
        error("%s: not a regular file", pathname);
    else
    {
        get_next_chunk(


        ptr = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (ptr == MAP_FAILED)
            error("cannot map %s: %s", pathname, ERRNO);
        else if (*((char*)ptr + info.st_size - 1) != '\n')
            error("%s: should end with a newline ('\\n')", pathname);
        else
            return (remove_duplicates(ptr, info.st_size));
    }

    close(fd);
    return (-1);
}
