#include <string.h>
#include <ctype.h>
#include "line.h"
#include "const.h"
#include "debug.h"
#include "chunk.h"
#include "config.h"


static bool str_isprint(char *str, size_t size)
{
    while (size--)
    {
        if (!isprint(str[size]))
            return (false);
    }
    return (true);
}


static bool line_filtered(char *str, size_t size)
{
    if (size > g_conf.line_max_size)
        return (true);
    else if (g_conf.filter_printable && !str_isprint(str, size))
        return (true);
    return (false);
}


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
        else if (size > 1 && *(uint16_t*)chunk->ptr == *(uint16_t*)"\r\n")
        {
            size -= 2;
            chunk->ptr += 2;
        }
        else if ((next = memchr(chunk->ptr, '\n', size)) == NULL)
        {
            if (line_filtered(chunk->ptr, size))
                return (false);
            SET_LINE(*dst, chunk->ptr, size);
            chunk->ptr += size;
            return (true);
        }
        else
        {
            line_size = next - chunk->ptr;
            if (chunk->ptr[line_size - 1] == '\r')
            {
                if (line_filtered(chunk->ptr, line_size - 1))
                {
                    chunk->ptr += line_size + 1;
                    size -= line_size + 1;
                }
                else
                {
                    SET_LINE(*dst, chunk->ptr, line_size - 1);
                    chunk->ptr += line_size + 1;
                    return (true);
                }
            }
            else
            {
                if (line_filtered(chunk->ptr, line_size))
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
        }
    }
    return (false);
}


int         cmp_line(t_line *l1, t_line *l2)
{
    int     size;

    size = LINE_SIZE(*l1);
    if (LINE_SIZE(*l2) != size)
        return (1);
    else
        return (memcmp(LINE_ADDR(*l1), LINE_ADDR(*l2), size));
}
