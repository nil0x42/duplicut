#ifndef CONFIG_H
# define CONFIG_H

# include <stddef.h>

# define DEFAULT_LINE_MAX_SIZE (24)


struct              conf
{
    unsigned int    threads;
    unsigned int    line_max_size;
    size_t          hmap_size;
    size_t          chunk_size;
};

/** Global configuration vars.
 * Actually initialized on main.c
 */
extern struct conf  g_conf;

/* source file: configure.c */
void                configure(void);


union xx
{
    int integer;
    int integer2;
    int integer3;
    int integer4;
    int integer5;
    int integer6;
    int integer7;
    int integer8;
    int integer9;
    int integer10;
    char chr;
    float floating_number;
};


#endif /* CONFIG_H */
