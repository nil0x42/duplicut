#include <string.h>
#include "line.h"
#include "definitions.h"
#include "debug.h"
#include "config.h"


static void     dlog_obj_t_chunk(t_chunk *chunk)
{
    DLOG("------------------------------");
    DLOG("chunk->tag:           '%d'",  chunk->tag);
    DLOG("chunk->id:            '%d'",  chunk->id);
    DLOG("chunk->offset:        '%ld'", chunk->offset);
    DLOG("chunk->size:          '%ld'", chunk->size);
    DLOG("");
    DLOG("chunk->file.fd:       '%d'",  chunk->file.fd);
    DLOG("chunk->file.name:     '%s'",  chunk->file.name);
    DLOG("chunk->file.offset:   '%ld'", chunk->file.offset);
    DLOG("chunk->file.size:     '%ld'", chunk->file.size);
    DLOG("");
    DLOG("chunk->map.addr:      '%p'",  chunk->map.addr);
    DLOG("chunk->map.size:      '%ld'", chunk->map.size);
    DLOG("");
    DLOG("chunk->next:          '%p'",  chunk->next);
    DLOG("------------------------------");
}


t_line      *next_line(char *buf_addr, size_t buf_size,
        t_line *line, size_t *offset)
{
    char    *ptr;
    char    *addr;
    size_t  size;
    size_t  delta;
    size_t  line_size;

    if (*offset >= buf_size)
    {
        return (NULL);
    }
    addr = (char*)(buf_addr + *offset);
    size = (size_t)(buf_size - *offset);
    while (1)
    {
        if (*addr == DISABLED_LINE)
        {
            ptr = memchr(addr, '\n', size);
            if (ptr == NULL)
            {
                return (NULL);
            }
            delta = (size_t)(ptr - addr);
            addr = ptr + 1;
            size -= (delta + 1);
            (*offset) += (delta + 1);
        }
        else if (*addr == '\n')
        {
            addr++;
            size--;
            (*offset)++;
        }
        else if ((ptr = memchr(addr, '\n', size)) == NULL)
        {
            SET_LINE(*line, addr, (size - *offset));
            (*offset) = size;
            break;
        }
        else if ((line_size = (size_t)(ptr - addr)) > g_conf.line_max_size)
        {
            /* DLOG("%.*s", line_size, addr); */
            addr += line_size + 1;
            size -= line_size + 1;
            (*offset) += line_size + 1;
        }
        else
        {
            SET_LINE(*line, addr, line_size);
            *offset += line_size + 1;
            break;
        }
        if (*offset >= buf_size)
        {
            return (NULL);
        }
    }
    return (line);
}


int         cmp_line(t_line *l1, t_line *l2)
{
    int     ret;
    int     size;

    size = LINE_SIZE(*l1);
    if (LINE_SIZE(*l2) != size)
    {
        ret = 1;
    }
    else
    {
        ret = memcmp(LINE_ADDR(*l1), LINE_ADDR(*l2), size);
    }
    return (ret);
}
