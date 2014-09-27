#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "config.h"
#include "chunk.h"
#include "exit.h"


static void         close_file(t_file *file)
{
    if (close(file->fd) < 0)
        die("close()");
}


static void         open_file(const char *pathname, t_file *file)
{
    struct stat     info;

    file->fd = open(pathname, O_RDONLY);
    if (file->fd < 0)
        error("cannot open %s: %s", pathname, ERRNO);
    else if (fstat(file->fd, &info) < 0)
    {
        close_file(file);
        error("cannot stat %s: %s", pathname, ERRNO);
    }
    else if (!S_ISREG(info.st_mode))
    {
        close_file(file);
        error("%s: not a regular file", pathname);
    }
    else
    {
        file->name = pathname;
        file->size = info.st_size;
        file->offset = 0;
        if (file->size == 0)
        {
            close_file(file);
            error("%s: file is empty");
        }
    }
}


/** Bind the given chunk to global chunk list (g_chunks).
 */
static void         attach_chunk(t_chunk *chunk)
{
    static t_chunk  *last_created_chunk = NULL;

    if (g_chunks == NULL)
        g_chunks = chunk;
    if (last_created_chunk != NULL)
        last_created_chunk->next = chunk;
    last_created_chunk = chunk;
}


/** Create a new chunk based on a portion of file.
 * This function appends the new allocated chunk
 * to `g_chunks`. It is created by copying
 * `size` bytes of `file->fd`, starting at `file->offset`
 * offset to a temporary file.
 */
t_chunk             *create_chunk(t_file *file, size_t size)
{
    t_chunk         *chunk;

    chunk = (t_chunk*) malloc(sizeof(t_chunk));
    if (chunk == NULL)
        return (NULL);
    memset(chunk, 0, sizeof(t_chunk));

    memcpy(&chunk->parent, file, sizeof(*file));
    chunk->map.size = size;
    strcpy(chunk->name, g_conf.tmpdir);
    strcat(chunk->name, CHUNK_FILENAME);
    if ((chunk->fd = mkstemps(chunk->name, 6)) < 0)
        error("cannot create chunk %s: %s", chunk->name, ERRNO);
    if (ftruncate(chunk->fd, chunk->map.size) < 0)
        error("cannot truncate chunk %s: %s", chunk->name, ERRNO);
    attach_chunk(chunk);
    return (chunk);
}


int                 chunkify_file(const char *pathname)
{
    t_file          file;
    t_chunk         *chunk;
    size_t          chunk_size;

    open_file(pathname, &file);
    while (file.offset < file.size)
    {
        if (g_conf.chunk_size > file.size - file.offset)
            chunk_size = file.size - file.offset;
        else
            chunk_size = g_conf.chunk_size;
        chunk = create_chunk(&file, chunk_size);
        if (chunk == NULL)
            abort();
        if (file.offset == 0)
            chunk->tag |= FIRST_CHUNK;
        file.offset += chunk_size;
        if (chunk_size == g_conf.chunk_size)
            file.offset -= g_conf.page_size;
        if (file.offset >= file.size)
            chunk->tag |= LAST_CHUNK;
        init_chunk(chunk);
    }
    close_file(&file);
    return (0);
}
