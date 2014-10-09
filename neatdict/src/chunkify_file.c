#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "config.h"
#include "chunk.h"
#include "vars.h"
#include "exit.h"


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
 * It appends the new allocated chunk to `g_chunks`.
 */
static t_chunk      *create_chunk(t_file *file, size_t size, t_chunk **list)
{
    t_chunk         *chunk;

    chunk = (t_chunk*) malloc(sizeof(t_chunk));
    if (chunk == NULL)
        die("not enough memory to allocate chunk");
    memset(chunk, 0, sizeof(t_chunk));
    attach_chunk(list, chunk);
    memcpy(&chunk->file, file, sizeof(*file));
    chunk->map.size = size;
    return (chunk);
}


/** Determine real bounaries for the given chunk.
 * (Because chunks are created from memory mappings, so
 * real bounaries shall be related to start/end of a
 * specific line of the wordlist).
 */
static void         bound_chunk(t_chunk *chunk)
{
    char            *ptr;
    size_t          delta;

    if (chunk->tag & FIRST_CHUNK)
    {
        delta = 0;
        chunk->addr = chunk->map.addr;
    }
    else if (chunk->map.addr[g_conf.page_size - 1] == '\n')
    {
        delta = g_conf.page_size;
        chunk->addr = &chunk->map.addr[g_conf.page_size];
    }
    else
    {
        chunk->addr = memrchr(chunk->map.addr, '\n', g_conf.page_size);
        if (chunk->addr == NULL)
            error("%s: can't find start bound", chunk->file.name);
        chunk->addr++;
        delta = (size_t)(chunk->addr - chunk->map.addr);
    }
    chunk->size = chunk->map.size - delta;
    if (!(chunk->tag & LAST_CHUNK))
    {
        if (chunk->addr[chunk->size - 1] == '\n')
            return ;
        else
        {
            ptr = memrchr(chunk->addr, '\n', chunk->size - 1);
            if (ptr == NULL)
                error("%s: can't find end bound", chunk->file.name);
            ptr++;
            delta = (size_t)(ptr - chunk->addr);
            chunk->size -= chunk->size - delta;
        }
    }
}


/** Split the given file into as many chunks as needed according
 * to current memory repartition rules.
 * Each chunk is allocated, initialized, and finally, attached
 * to the end of `chunk_list`
 */
int                 chunkify_file(const char *pathname, t_chunk **chunk_list)
{
    t_file          file;
    t_chunk         *chunk;
    size_t          chunk_size;

    /* printf("\r%d chunk(s) loaded ...", g_vars.num_chunks); */
    /* fflush(stdout); */
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
        bound_chunk(chunk);
        /* printf("\r%d chunk(s) loaded ...", g_vars.num_chunks); */
        /* fflush(stdout); */
    }
    close_file(&file);
    return (0);
}
