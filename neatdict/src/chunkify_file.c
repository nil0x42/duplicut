#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "config.h"
#include "chunk.h"
#include "vars.h"
#include "exit.h"
#include "debug.h"


/** Close the given `file` struct.
 */
static void         close_file(t_file *file)
{
    if (close(file->fd) < 0)
        die("close()");
}


/** Open `pathname` and write related informations to `file`
 */
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


/** Attach `chunk` to the end of `chunk_list`.
 */
static void         attach_chunk(t_chunk **chunk_list, t_chunk *chunk)
{
    t_chunk         *chunk_pos;

    g_vars.num_chunks++;
    if (*chunk_list == NULL)
        *chunk_list = chunk;
    else
    {
        chunk_pos = *chunk_list;
        while (chunk_pos->next != NULL)
            chunk_pos = chunk_pos->next;
        chunk_pos->next = chunk;
    }
}


/** Create a new chunk based on a portion of file.
 * This function appends the new allocated chunk
 * to `g_chunks`. It is created by copying
 * `size` bytes of `file->fd`, starting at `file->offset`
 * offset to a temporary file.
 */
static t_chunk      *create_chunk(t_file *file, size_t size, t_chunk **list)
{
    t_chunk         *chunk;

    chunk = (t_chunk*) malloc(sizeof(t_chunk));
    if (chunk == NULL)
        die("not enough memory to allocate chunk");
    memset(chunk, 0, sizeof(t_chunk));
    attach_chunk(list, chunk);
    memcpy(&chunk->parent, file, sizeof(*file));
    chunk->map.size = size;
    strcpy(chunk->name, g_conf.tmpdir);
    strcat(chunk->name, CHUNK_FILENAME);
    if ((chunk->fd = mkstemps(chunk->name, 6)) < 0)
        error("cannot create chunk %s: %s", chunk->name, ERRNO);
    if (ftruncate(chunk->fd, chunk->map.size) < 0)
        error("cannot truncate chunk %s: %s", chunk->name, ERRNO);
    return (chunk);
}


/** Create and initialize as many chunks as needed from
 * `pathname` file, and attach them to the end of `chunk_list`
 */
int                 chunkify_file(const char *pathname, t_chunk **chunk_list)
{
    t_file          file;
    t_chunk         *chunk;
    size_t          chunk_size;

    printf("\r%d chunk(s) loaded ...", g_vars.num_chunks);
    fflush(stdout);
    open_file(pathname, &file);
    while (file.offset < file.size)
    {
        if (g_conf.chunk_size > file.size - file.offset)
            chunk_size = file.size - file.offset;
        else
            chunk_size = g_conf.chunk_size;
        chunk = create_chunk(&file, chunk_size, chunk_list);
        if (file.offset == 0)
            chunk->tag |= FIRST_CHUNK;
        file.offset += chunk_size;
        if (chunk_size == g_conf.chunk_size)
            file.offset -= g_conf.page_size;
        if (file.offset >= file.size)
            chunk->tag |= LAST_CHUNK;
        init_chunk(chunk);
        printf("\r%d chunk(s) loaded ...", g_vars.num_chunks);
        fflush(stdout);
    }
    close_file(&file);
    return (0);
}
