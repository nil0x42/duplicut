#ifndef LINE_H
# define LINE_H

# include "chunk.h"

#define LINE_SIZE(line)       ((uint8_t)line)
#define LINE_ADDR(line)       ((char*)(line >> 8))
#define MAKE_LINE(addr, size) ((((uintptr_t)addr) << 8) + (uint8_t)size)

# else

typedef struct  s_line
{
    char        *addr;
    int         size;
}               t_line;

# endif

# define LINE_SIZE(l)        ((int)((l).size))
# define LINE_ADDR(l)        ((char*)((l).addr))

# define SET_LINE_SIZE(l, v) ((l).size = v)
# define SET_LINE_ADDR(l, v) ((l).addr = v)


typedef struct  s_line
{
    char        *addr;
    int         size;
}               t_line;

// line_utils.c
int             cmp_line(t_line *l1, t_line *l2);
t_line          *next_line(t_line *line, t_chunk *chunk, size_t *offset);

#endif /* LINE_H */

/* POC */
// #include <stdlib.h>
// #include <stdio.h>
// #include <stdint.h>
//
// #define DATA_SIZE(data)     ((uint8_t)data)
// #define DATA_ADDR(data)     ((char*)(data >> 8))
// #define TO_DATA(addr, size) ((((uintptr_t)addr) << 8) + (uint8_t)size)
//
//
// int main(void)
// {
//     size_t      data;
//     void        *addr;
//     int         size;
//
//     addr = malloc(200);
//     size = 231;
//     data = TO_DATA(addr, size);
//     printf("orig addr: '%p'\n", addr);
//     printf("new addr:  '%p'\n", DATA_ADDR(data));
//     printf("\n");
//     printf("orig size: '%d'\n", size);
//     printf("new size:  '%d'\n", DATA_SIZE(data));
//     return (0);
// }
