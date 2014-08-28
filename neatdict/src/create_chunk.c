#include <unistd.h>
#include <string.h>
#include "definitions.h"
#include "exit.h"

extern t_chunk      *g_chunks;

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
#define TMPFILE ("/home/nil/.neatdict-tmp/neatdict-XXXXXX.chunk")
t_chunk             *create_chunk(t_file *file, size_t size)
{
    t_chunk         *chunk;

    if ((chunk = (t_chunk*) malloc(sizeof(t_chunk))) == NULL)
        return (NULL);
    memset(chunk, 0, sizeof(t_chunk));

    memcpy(&chunk->file, file, sizeof(*file));
    chunk->size = size;
    strcpy(chunk->name, TMPFILE);
    if ((chunk->fd = mkstemps(chunk->name, 6)) < 0)
        error("cannot create chunk %s: %s", chunk->name, ERRNO);
    if (ftruncate(chunk->fd, chunk->size) < 0)
        error("cannot truncate chunk %s: %s", chunk->name, ERRNO);
    attach_chunk(chunk);
    return (chunk);
}
