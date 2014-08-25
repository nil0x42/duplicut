#ifndef CHUNK_H
# define CHUNK_H

# include <stdlib.h>
# include <stddef.h>

typedef struct      s_chunk
{
    void            *ptr;
    size_t          len;
    struct s_chunk  *next;
}                   t_chunk;

#endif
