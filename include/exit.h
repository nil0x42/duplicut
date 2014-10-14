#ifndef ERROR_H
# define ERROR_H

# include <string.h>
# include <errno.h>

# define ERRNO (strerror(errno))

void        exit_properly(int status);
void        error(const char *fmt, ...);
void        die(const char *msg);

#endif
