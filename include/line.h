#ifndef LINE_H
# define LINE_H

# include <stddef.h>
# include <stdint.h>
# include "chunk.h"

typedef size_t  t_line;

#  define LINE_ISSET(ln)        (ln != 0)
#  define LINE_SIZE(ln)         ((uint8_t)ln)
#  define LINE_ADDR(ln)         ((char*)(ln >> 8))
#  define SET_LINE(ln, ptr, sz) (ln = ((((uintptr_t)ptr) << 8) + (uint8_t)sz))

// line_utils.c
int         cmp_line(t_line *l1, t_line *l2);
t_line      *next_line(char *buf_addr, size_t buf_size,
        t_line *line, size_t *offset);

#endif /* LINE_H */
