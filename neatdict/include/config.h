#ifndef CONFIG_H
# define CONFIG_H

# include <stddef.h>
# include "definitions.h"

# define DEFAULT_TMPDIR "/tmp"

typedef struct  s_conf
{
    long        memlimit;
    const char  *tmpdir;
    int         threads;
    int         page_size;
    size_t      hmap_size;
    size_t      chunk_size;
}               t_conf;

extern t_conf   g_conf;

// config.c
void            configure(void);

// optparse.c
void            optparse(int argc, char **argv, int *idx);

#endif /* CONFIG_H */
