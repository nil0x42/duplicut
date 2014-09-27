#ifndef CHUNK_H
# define CHUNK_H

# include "definitions.h"

# define FIRST_CHUNK    (1)
# define LAST_CHUNK     (2)

typedef struct      s_file
{
    int             fd;
    const char      *name;
    size_t          size;
    size_t          offset;
}                   t_file;

typedef struct      s_map
{
    char            *addr;
    size_t          size;
}                   t_map;

typedef struct      s_chunk
{
    int             fd;
    int             tag;
    char            name[CHUNK_PATHSIZE];
    char            *addr;
    size_t          size;
    t_file          parent;
    t_map           map;
    struct s_chunk  *next;
}                   t_chunk;

extern t_chunk      *g_chunks;

// chunkify_file.c
int                 chunkify_file(const char *pathname);

// init_chunk.c
void                init_chunk(t_chunk *chunk);

#endif /* CHUNK_H */
