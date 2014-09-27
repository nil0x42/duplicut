#ifndef LINE_H
# define LINE_H

#include "chunk.h"

typedef struct  s_line
{
    char        *addr;
    int         size;
}               t_line;

// line_utils.c
int             cmp_line(t_line *l1, t_line *l2);
t_line          *next_line(t_line *line, t_chunk *chunk, size_t *offset);

#endif /* LINE_H */
