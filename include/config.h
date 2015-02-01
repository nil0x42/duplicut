#ifndef CONFIG_H
# define CONFIG_H

# include <stddef.h>

# define DEFAULT_LINE_MAX_SIZE (24)

typedef struct  s_conf
{
    long        memlimit;
    int         threads;
    size_t      line_max_size;
    int         page_size;
    size_t      hmap_size;
    size_t      chunk_size;
}               t_conf;

/** Global configuration vars.
 * Actually initialized on main.c
 */
extern t_conf   g_conf;

/* source file: configure.c */
void            configure(void);

#endif /* CONFIG_H */
