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

static t_chunk  *add_chunk(t_file *file, t_map *map, int id)
{
    static t_chunk  *last = NULL;
    t_chunk         *chunk;

    chunk = (t_chunk*) malloc(sizeof(t_chunk));
    if (chunk == NULL)
        return (NULL);
    chunk->ptr = NULL;
    chunk->size = 0;
    chunk->next = NULL;
    chunk->id = id;
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


# define MAP(fd, off, len) mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, off)
void            map_chunk(t_chunk *chunk)
{
    chunk->ptr = MAP(chunk->file.fd, chunk->map.offset, chunk->map.size);
    if (chunk->ptr == MAP_FAILED)
        die("%s: could not map chunk %d", chunk->file.name, chunk->id);
}


# define PAGE_SIZE     (4096)
# define CHUNK_SIZE    (PAGE_SIZE * 3)
int             get_chunks(const char *pathname, t_chunk **chunklist)
{
    t_file          file;
    t_map           map;
    t_chunk         *chunk;
    int             id;

    if (open_file(pathname, &file) < 0)
        return (-1);
    id = 1;
    map.offset = 0;
    while (map.offset < file.size)
    {
        if (CHUNK_SIZE > file.size - map.offset)
            map.size = file.size - map.offset;
        else
            map.size = CHUNK_SIZE;
        if ((chunk = add_chunk(&file, &map, id)) == NULL)
        {
            puts(ERRNO);
            abort();
        }
        map_chunk(chunk);
        if (*chunklist == NULL)
            *chunklist = chunk;
        map.offset += map.size;
        if (map.size == CHUNK_SIZE)
            map.offset -= PAGE_SIZE;
        ++id;
    }
}
