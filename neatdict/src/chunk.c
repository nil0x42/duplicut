#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "definitions.h"
#include "debug.h"
#include "error.h"

static int      open_file(const char *pathname, t_file *file)
{
    struct stat     info;

    if ((file->fd = open(pathname, O_RDONLY)) < 0)
        error("cannot open %s: %s", pathname, ERRNO);
    else if (fstat(file->fd, &info) < 0)
        error("cannot stat %s: %s", pathname, ERRNO);
    else if (!S_ISREG(info.st_mode))
        error("%s: not a regular file", pathname);
    else
    {
        file->name = pathname;
        file->size = info.st_size;
        return (0);
    }
    return (-1);
}

static t_chunk  *add_chunk(t_file *file, t_map *map)
{
    static t_chunk  *last = NULL;
    t_chunk         *chunk;

    chunk = (t_chunk*) malloc(sizeof(t_chunk));
    if (chunk == NULL)
        return (NULL);
    // can be done with only one memcpy
    memcpy(&chunk->file, file, sizeof(*file));
    memcpy(&chunk->map, map, sizeof(*map));
    if (last == NULL)
        last = chunk;
    else
    {
        last->next = chunk;
        last = last->next;
    }
    return (chunk);
}

#define MAP(off, len) (mmap(NULL, len, PROT_READ, MAP_PRIVATE, file.fd, off))
# define PAGE_SIZE    (4096)
# define CHUNK_SIZE   (PAGE_SIZE * 3)
int             get_chunks(const char *pathname, t_chunk **chunklist)
{
    t_file          file;
    t_map           map;
    t_chunk         *chunk;

    if (open_file(pathname, &file) < 0)
        return (-1);
    map.offset = 0;
    while (map.offset < file.size)
    {
        if (CHUNK_SIZE > file.size - map.offset)
            map.size = file.size - map.offset;
        else
            map.size = CHUNK_SIZE;
        if ((chunk = add_chunk(&file, &map)) == NULL)
        {
            puts(ERRNO);
            abort();
        }
        if (*chunklist == NULL)
            *chunklist = chunk;
        map.offset += map.size;
        if (map.size == CHUNK_SIZE)
            map.offset -= PAGE_SIZE;
    }
}
