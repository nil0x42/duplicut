#include <string.h>
#include "line.h"


t_line      *next_line(t_line *line, t_chunk *chunk, size_t *offset)
{
    char    *ptr;
    char    *addr;
    size_t  size;
    size_t  delta;

    if (*offset >= chunk->size)
        return (NULL);
    addr = (char*)(chunk->addr + *offset);
    size = (size_t)(chunk->size - *offset);
    while (1)
    {
        if (*addr == DISABLED_LINE)
        {
            ptr = memchr(addr, '\n', size);
            if (ptr == NULL)
                return (NULL);
            delta = (size_t)(ptr - addr);
            addr = ptr + 1;
            size -= (delta + 1);
            *offset += (delta + 1);
        }
        else if (*addr == '\n')
        {
            addr++;
            size--;
            (*offset)++;
        }
        else
            break;
        if (*offset >= chunk->size)
            return (NULL);
    }
    ptr = memchr(addr, '\n', size);
    if (ptr == NULL)
    {
        SET_LINE(*line, addr, (size - *offset));
        *offset = size;
    }
    else
    {
        SET_LINE(*line, addr, (ptr - addr));
        *offset += LINE_SIZE(*line) + 1;
    }
    return (line);
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
