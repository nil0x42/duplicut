#include <string.h>
#include <ctype.h>
#include "line.h"
#include "const.h"
#include "debug.h"
#include "chunk.h"
#include "config.h"


/** True if `str` only contains printable chars
 */
static bool str_isprint(char *str, size_t size)
{
    while (size--)
    {
        if (!isprint(str[size]))
            return (false);
    }
    return (true);
}


/** True if `str` was filtered by line removal rules
 */
static bool filter_line(char *str, size_t size)
{
    if (size > g_conf.line_max_size)
        return (true);
    else if (g_conf.filter_printable && !str_isprint(str, size))
        return (true);
    return (false);
}


/** Set `dst` to next line in `chunk`
 * The function returns false when end-of-chunk is reached.
 * You are not expected to understand this
 */
bool        get_next_line(t_line *dst, t_chunk *chunk, size_t *junk_lines)
{
    size_t  size;
    char    *next;
    size_t  line_size;

    size = chunk->endptr - chunk->ptr;

    while (size > 0)
    {
        /* ptr at '\0' (DISABLED_LINE) */
        if (chunk->ptr[0] == DISABLED_LINE)
        {
            ++*junk_lines;
            if ((next = memchr(chunk->ptr, '\n', size)) == NULL)
                return (false);
            size -= (next - chunk->ptr) + 1;
            chunk->ptr = next + 1;
        }
        /* ptr at '\n' */
        else if (chunk->ptr[0] == '\n')
        {
            ++*junk_lines;
            size -= 1;
            chunk->ptr += 1;
        }
        /* ptr at '\r\n' */
        else if (chunk->ptr[0] == '\r' && size > 1 && chunk->ptr[1] == '\n')
        {
            ++*junk_lines;
            size -= 2;
            chunk->ptr += 2;
        }
        /* ptr at last line with no newline in the end */
        else if ((next = memchr(chunk->ptr, '\n', size)) == NULL)
        {
            if (filter_line(chunk->ptr, size))
            {
                ++*junk_lines;
                return (false);
            }
            SET_LINE(*dst, chunk->ptr, size);
            chunk->ptr += size;
            return (true);
        }
        else
        {
            line_size = next - chunk->ptr;
            if (chunk->ptr[line_size - 1] == '\r')
            {
                if (filter_line(chunk->ptr, line_size - 1))
                {
                    ++*junk_lines;
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
                if (filter_line(chunk->ptr, line_size))
                {
                    ++*junk_lines;
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


/** Compare two t_line objects.
 * If the function returns 0, then lines are equal
 */
int         cmp_line(t_line *l1, t_line *l2)
{
    int     size;

    size = LINE_SIZE(*l1);
    if (LINE_SIZE(*l2) != size)
        return (1);
    else
        return (memcmp(LINE_ADDR(*l1), LINE_ADDR(*l2), size));
}
