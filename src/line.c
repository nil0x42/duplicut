#include <string.h>
#include "line.h"
#include "const.h"
#include "debug.h"
#include "chunk.h"
#include "config.h"


bool        get_next_line(t_line *dst, t_chunk *chunk)
{
    size_t  size;
    char    *next;
    size_t  line_size;

    size = chunk->endptr - chunk->ptr;
    while (size > 0)
    {
        if (chunk->ptr[0] == DISABLED_LINE)
        {
            if ((next = memchr(chunk->ptr, '\n', size)) == NULL)
                return (false);
            size -= (next - chunk->ptr) + 1;
            chunk->ptr = next + 1;
        }
        else if (chunk->ptr[0] == '\n')
        {
            size -= 1;
            chunk->ptr += 1;
        }
        else if ((next = memchr(chunk->ptr, '\n', size)) == NULL)
        {
            SET_LINE(*dst, chunk->ptr, size);
            return (true);
        }
        else if ((line_size = next - chunk->ptr) > g_conf.line_max_size)
        {
            chunk->ptr += line_size + 1;
            size -= line_size + 1;
        }
        else
        {
            SET_LINE(*dst, chunk->ptr, line_size);
            chunk->ptr += line_size + 1;
            return (true);
        }
    }
    return (false);
}


int         cmp_line(t_line *l1, t_line *l2)
{
    int     ret;
    int     size;

    size = LINE_SIZE(*l1);

    if (LINE_SIZE(*l2) != size)
        ret = 1;
    else
        ret = memcmp(LINE_ADDR(*l1), LINE_ADDR(*l2), size);

    return (ret);
}
