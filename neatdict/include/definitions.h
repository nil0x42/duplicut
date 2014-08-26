#ifndef DEFINITIONS_H
# define DEFINITIONS_H

# include <stdlib.h>

# define PROGNAME "neatdict"
# define true (1)

typedef struct      s_file
{
    const char      *name;
    size_t          size;
    int             fd;
}                   t_file;

typedef struct      s_map
{
    void            *ptr;
    size_t          size;
    size_t          offset;
}                   t_map;

typedef struct      s_chunk
{
    void            *ptr;
    size_t          size;
    struct s_chunk  *next;
    int             id;
    t_map           map;
    t_file          file;
}                   t_chunk;


/*
 * memory_map.c
 */
int              memory_map(const char *pathname);

/*
 * remove_duplicates.c
 */
int              remove_duplicates(void *ptr, off_t off);

/*
 * chunk.c
 */
int              get_chunks(const char *pathname, t_chunk **head);

#endif
