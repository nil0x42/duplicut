#ifndef ERROR_H
# define ERROR_H

# define ERRNO (strerror(errno))

void        exit_properly(int status);
void        error(const char *fmt, ...);
void        die(const char *msg);

#endif
