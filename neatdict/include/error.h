#ifndef DEBUG_H
# define DEBUG_H

# include <unistd.h>
# include <stdio.h>
# include <string.h>
# include <errno.h>
# include <stdarg.h>
# include "definitions.h"

# define ERRNO (strerror(errno))

int        die(const char *fmt, ...);
int        error(const char *fmt, ...);

#endif
