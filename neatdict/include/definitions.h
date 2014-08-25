#ifndef DEFINITIONS_H
# define DEFINITIONS_H

# include <stdlib.h>

# define PROGNAME "neatdict"
# define true (1)

/*
 * memory_map.c
 */
int             memory_map(const char *pathname);

/*
 * remove_duplicates.c
 */
int             remove_duplicates(void *ptr, off_t off);

#endif
