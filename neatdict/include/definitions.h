#ifndef DEFINITIONS_H
# define DEFINITIONS_H

# include <stdlib.h>
# include <unistd.h>

# define true (1)

# define PROGNAME         "neatdict"
# define PROJECT_VERSION  "0.1a"
# define PROJECT_URL      "http://github.com/nil0x42/neatcrack"


# define HMAP_LOAD_FACTOR   (0.5)
# define MEDIUM_LINE_BYTES  (5)          // a line takes at least ~= 5 bytes
# define KEEP_FREE_MEMORY   (0xc800000L) // 200MB RAM kept unused as security


# define FIRST_CHUNK    (1)
# define LAST_CHUNK     (2)

# define DEFAULT_TMPDIR "/tmp"
# define CHUNK_FILENAME "/neatdict-XXXXXX.chunk"
# define CHUNK_PATHSIZE (256)

# define DISABLED_LINE  '\0'

/* should be removed */
# include "debug.h"

typedef struct      s_conf
{
    size_t          memlimit;
    int             threads;
    const char      *tmpdir;
    int             page_size;
    size_t          hmap_size;
    size_t          chunk_size;
}                   t_conf;

typedef struct      s_file
{
    int             fd;
    const char      *name;
    size_t          offset;
    size_t          size;
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


typedef struct      s_line
{
    char            *addr;
    int             size;
}                   t_line;

// configure.c
void        configure(int argc, char **argv, int *idx);

// line_utils.c
int         linecmp(t_line *l1, t_line *l2);
t_line      *get_next_line(t_line *line, t_chunk *chunk, size_t *offset);

// memory_map.c
int         memory_map(const char *pathname);

// remove_duplpicates.c
int         remove_duplicates(char *ptr, off_t off);

// get_available_memory.c
long        get_available_memory(void);

// chunk handlers
t_chunk     *create_chunk(t_file *file, size_t size);
void        init_chunk(t_chunk *chunk);
int         chunkify_file(const char *pathname);

// chunk handlers for debugging purposes
void        display_chunk_infos(t_chunk *chunk);
void        output_chunk(t_chunk *chunk);



#endif
