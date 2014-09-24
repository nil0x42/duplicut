#include <string.h>
#include "definitions.h"


int         linecmp(t_line *l1, t_line *l2)
{
    int     ret;

    if (l1->size != l2->size)
        ret = 1;
    else
        ret = memcmp(l1->addr, l2->addr, l1->size);
    return (ret);
}


t_line      *get_next_line(t_line *line, t_chunk *chunk, size_t *offset)
{
    char    *ptr;
    char    *addr;
    size_t  size;
    size_t  delta;

    if (*offset >= chunk->size)
        return (NULL);

    addr = (char*)(chunk->addr + *offset);
    size = (size_t)(chunk->size - *offset);

    while (true)
    {
        if (*addr == DISABLED_LINE)
        {
            ptr = memchr(addr, '\n', size);
            if (ptr == NULL)
                return (NULL);
            delta = (size_t)(ptr - addr);
            addr = ptr + 1;
            size -= (delta + 1);
            offset += (delta + 1);
        }
        else if (*addr == '\n')
        {
            addr++;
            size--;
            offset++;
        }
        else
            break;
        if (*offset >= chunk->size)
            return (NULL);
    }

    ptr = memchr(addr, '\n', size);
    if (ptr == NULL)
    {
        line->addr = addr;
        line->size = size - *offset;
        *offset = size;
    }
    else
    {
        line->addr = addr;
        line->size = ptr - addr;
        *offset += line->size + 1;
    }


}


void        read_chunk_lines(t_chunk *chunk)
{
    t_line  line;
    size_t  offset;

    offset = 0;
    while (get_next_line(&line, chunk, &offset) != NULL)
    {
        write(1, line.addr, line.size);
        write(1, "\n", 1);
    }
}
