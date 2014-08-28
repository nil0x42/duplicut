#ifndef DEFINITIONS_H
# define DEFINITIONS_H

# include <stdlib.h>
# include <unistd.h>
# include "debug.h"

# define PROGNAME "neatdict"
# define true (1)

typedef struct      s_file
{
    int             fd;
    const char      *name;
    size_t          offset;
    size_t          size;
}                   t_file;

typedef struct      s_chunk
{
    int             fd;
    char            name[255];
    void            *ptr;
    size_t          size;
    t_file          file;
    struct s_chunk  *next;
}                   t_chunk;


/*
 * memory_map.c
 */
int              memory_map(const char *pathname);

/*
 * remove_duplicates.c
 */
int              remove_duplicates(void *ptr, off_t off);


t_chunk             *create_chunk(t_file *file, size_t size);
int                 chunkify_file(const char *pathname);

#endif
