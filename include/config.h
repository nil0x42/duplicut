#ifndef CONFIG_H
# define CONFIG_H

# include <stddef.h>
# include <limits.h>

# define DEFAULT_LINE_MAX_SIZE (16)

typedef struct  s_conf
{
    long        memlimit;
    int         threads;
    size_t      line_max_size;
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
