#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "chunk.h"
#include "line.h"
#include "error.h"
#include "debug.h"


/** Rewrite file without invalid lines, and return new file size
 */
static size_t   cleanout_file(char *file_addr, size_t file_size)
{
    char        *src;
    char        *dst;
    size_t      new_size;
    size_t      offset;
    t_line      line;
    size_t      line_size;

    new_size = 0;
    offset = 0;
    dst = file_addr;
    while (next_line(file_addr, file_size, &line, &offset) != NULL)
    {
        src = LINE_ADDR(line);
        line_size = LINE_SIZE(line);
        /* write(1, src, line_size); */
        /* write(1, "\n", 1); */
        memcpy(dst, src, line_size);
        dst += line_size;
        new_size += line_size;
        // add newline if possible
        if (new_size != file_size)
        {
            *dst = '\n';
            dst++;
            new_size++;
        }
    }
    DLOG("old size = %ld", file_size);
    DLOG("new size = %ld", new_size);
    return (new_size);
}


void            remove_duplicates(t_chunk *chunk)
{
    char        *pathname;
    char        *addr;
    size_t      new_size;

    while (chunk != NULL)
    {
        pathname = (char*) chunk->file.name;
        addr = mmap(NULL, chunk->file.size, (PROT_READ | PROT_WRITE),
                MAP_SHARED, chunk->file.fd, 0);
        if (addr == MAP_FAILED)
        {
            error("cannot cleanout %s tags: mmap(): %s",
                    pathname, ERRNO);
        }
        new_size = cleanout_file(addr, chunk->file.size);
        if (ftruncate(chunk->file.fd, new_size) < 0)
        {
            error("cannot cleanout %s tags: ftruncate(): %s",
                    pathname, ERRNO);
        }
        if (munmap(addr, chunk->file.size) < 0)
        {
            error("cannot cleanout %s tags: munmap(): %s",
                    pathname, ERRNO);
        }
        while (chunk != NULL && chunk->file.name == pathname)
        {
            chunk = chunk->next;
        }
    }
}
