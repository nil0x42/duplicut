#ifndef CHUNK_H
# define CHUNK_H

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
    // int             fd;
    // char            name[CHUNK_PATHSIZE];
    int             id;
    int             tag;
    // char            *addr;
    size_t          offset;
    size_t          size;
    t_file          file;
    t_map           map;
    struct s_chunk  *next;
}                   t_chunk;


// chunk.c
void                load_chunk(t_chunk *chunk);
void                unload_chunk(t_chunk *chunk);

// bound_chunk.c
void                bound_chunk(t_chunk *chunk);

// chunkify_file.c
int                 chunkify_file(const char *pathname, t_chunk **chunk_list);

// tag_duplicates.c
void                tag_duplicates(t_chunk *main_chunk);

// remove_duplicates.c
void                remove_duplicates(t_chunk *chunk);

#endif /* CHUNK_H */
