#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "chunk.h"
#include "file.h"
#include "config.h"
#include "debug.h"
#include "error.h"


/** Locate boundaries without making future chunks resident. */
static char *find_newline(char *ptr, char *endptr, struct file *file)
{
    char    buffer[0x1000];
    char    *next;
    size_t  size;
    ssize_t result;

    if (!g_conf.mmap_window)
        return (memchr(ptr, '\n', endptr - ptr));
    while (ptr < endptr)
    {
        size = endptr - ptr;
        if (size > sizeof(buffer))
            size = sizeof(buffer);
        do {
            result = pread(file->fd, buffer, size, ptr - file->addr);
        } while (result < 0 && errno == EINTR);
        if (result <= 0)
            error("cannot read chunk boundary: %s", result < 0 ? ERRNO : "unexpected EOF");
        next = memchr(buffer, '\n', result);
        if (next != NULL)
            return (ptr + (next - buffer));
        ptr += result;
    }
    return (NULL);
}


/** Restrict a scan to the configured mapped window, ending after a newline. */
t_chunk     get_chunk_window(const t_chunk *range)
{
    t_chunk chunk = *range;
    char    *end;

    if (!g_conf.mmap_window
            || (size_t)(range->endptr - range->ptr) <= g_conf.mmap_window)
        return (chunk);
    end = range->ptr + g_conf.mmap_window;
    end = find_newline(end, range->endptr, g_file);
    chunk.endptr = end ? end + 1 : range->endptr;
    return (chunk);
}


/** Release resident file pages while preserving their shared contents. */
void        release_chunk(const t_chunk *chunk)
{
    uintptr_t start;
    uintptr_t end;

    if (!g_conf.mmap_window)
        return;
    start = (uintptr_t)chunk->ptr / g_conf.page_size * g_conf.page_size;
    end = (uintptr_t)chunk->endptr / g_conf.page_size * g_conf.page_size;
    if (end > start && madvise((void*)start, end - start, MADV_DONTNEED) < 0)
    {
        DLOG1("cannot release mapped file pages: %s", ERRNO);
    }
}


/** Write next chunk from `file` into `chunk`
 * If `chunk->ptr` is NULL, chunk is set to `file`'s first chunk.
 * Otherwise, `chunk` is replaced by the next one.
 * If there is no next chunk, the function returns false.
 */
bool        get_next_chunk(t_chunk *chunk, struct file *file)
{
    char    *ptr;
    char    *endptr;
    ssize_t remaining_size;

    if (chunk->endptr == file->addr + file->info.st_size)
    {
        memset(chunk, 0, sizeof(t_chunk));
        return (false);
    }

    // IS FIRST
    if (chunk->ptr == NULL)
    {
        ptr = file->addr;
        remaining_size = file->info.st_size;
    }
    // IS NOT FIRST
    else
    {
        ptr = chunk->endptr;
        remaining_size = file->info.st_size - (ptr - file->addr);
    }

    // IS LAST
    if (remaining_size <= (ssize_t) g_conf.chunk_size)
    {
        endptr = ptr + remaining_size;
        remaining_size = 0;
    }
    // IS NOT LAST
    else
    {
        endptr = ptr + g_conf.chunk_size;
        remaining_size -= g_conf.chunk_size;
        if ((endptr = find_newline(endptr, file->addr + file->info.st_size, file)) == NULL)
        {
            endptr = file->addr + file->info.st_size;
            remaining_size = 0;
        }
        else
        {
            endptr += 1; // set endptr AFTER the \n
            remaining_size -= endptr - (ptr + g_conf.chunk_size);
        }
    }

    chunk->ptr = ptr;
    chunk->endptr = endptr;
    DLOG4("get_next_chunk() returns ptr=%p, endptr=%p, size=%zd, remaining_size=%zd",
            chunk->ptr,
            chunk->endptr,
            chunk->endptr - chunk->ptr,
            remaining_size
         );
    return (true);
}
