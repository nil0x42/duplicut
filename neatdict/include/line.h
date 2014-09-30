#ifndef LINE_H
# define LINE_H

# include <stdint.h>
# include "chunk.h"

typedef size_t  t_line;

#  define LINE_SIZE(line)    ((uint8_t)line)
#  define LINE_ADDR(line)    ((char*)(line >> 8))
#  define MAKE_LINE(ptr, sz) ((size_t)((((uintptr_t)ptr) << 8) + (uint8_t)sz))

// line_utils.c
int             cmp_line(t_line *l1, t_line *l2);
t_line          *next_line(t_line *line, t_chunk *chunk, size_t *offset);

#endif /* LINE_H */
