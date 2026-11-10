#include <string.h>
#include "dedupe.h"
#include "config.h"
#include "file.h"
#include "chunk.h"
#include "line.h"
#include "mmap.h"
#include "partition.h"
#include "status.h"


void            tag_duplicates(void)
{
    if (!g_conf.partition_count || !tag_partitions())
        tag_chunks();
}


/** Rewrite file, ignoring lines tagged with 'DISABLED_LINE'
 */
void            remove_duplicates(void)
{
    t_chunk     file_chunk;
    t_chunk     range;
    t_chunk     mapped;
    char        *output_start;
    char        *line_ptr;
    int         line_sz;
    char        *dst;
    char        *base_ptr;
    int         i;

    file_chunk.ptr = g_file->addr;
    file_chunk.endptr = g_file->addr + g_file->info.st_size;

    i = 0;
    base_ptr = file_chunk.ptr;

    dst = file_chunk.ptr;
    size_t junk_lines = 0;
    range = file_chunk;
    while (range.ptr < range.endptr)
    {
        file_chunk = get_chunk_window(&range);
        mapped = file_chunk;
        output_start = dst;
        while (get_next_line(&line_ptr, &line_sz, &file_chunk, &junk_lines))
        {
            memmove(dst, line_ptr, line_sz);
            dst += line_sz;
            if (dst != range.endptr)
                *dst++ = '\n';
            i++;
            if (i == 500000) {
                set_status(FCLEAN_BYTES, (size_t)(file_chunk.ptr - base_ptr));
                base_ptr = file_chunk.ptr;
                i = 0;
            }
        }
        release_chunk(&mapped);
        release_chunk(&(t_chunk){output_start, dst});
        range.ptr = mapped.endptr;
    }

    /* Count filtered lines once, excluding the duplicates already reported. */
    set_status(FCLEAN_SKIPPED, junk_lines);

    /* update file size */
    g_file->info.st_size = dst - g_file->addr;
    set_status(FCLEAN_BYTES, (size_t)(range.endptr - base_ptr));
}
