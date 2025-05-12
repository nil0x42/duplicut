#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

/** Ignore warning:
 * ignoring return value of ‘write’, declared with attribute warn_unused_result
 * Because we write to STDERR_FILENO & don't need to check write() return.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"


/* Internal helper that writes the buffer to STDERR in a single syscall. */
static inline void     err_write(const char *buf, size_t len)
{
    write(STDERR_FILENO, buf, len);
}

/** Print a formatted warning message without leaving the program.
 * The use of printf() inside this function causes it
 * to potentially allocate memory.
 * Unlike error() and die(), this function do not leave the program,
 * making it safe for use in atexit() callbacks.
 */
void            warning(const char *fmt, ...)
{
    char        buf[4096];
    size_t      offset = 0;
    va_list     ap;

    /* Prefix */
    offset += snprintf(buf + offset, sizeof(buf) - offset, "\nwarning: ");
    /* User‑provided format */
    va_start(ap, fmt);
    offset += vsnprintf(buf + offset, sizeof(buf) - offset, fmt, ap);
    va_end(ap);
    /* Trailing newline */
    offset += snprintf(buf + offset, sizeof(buf) - offset, "\n");
    err_write(buf, offset);
}


/** Print a formatted error message and leave the program.
 * The use of printf() inside this function causes it
 * to potentially allocate memory.
 * Memory allocation errors shall then use die() instead.
 */
void            error(const char *fmt, ...)
{
    char        buf[4096];
    size_t      offset = 0;
    va_list     ap;

    /* Prefix */
    offset += snprintf(buf + offset, sizeof(buf) - offset, "\nerror: ");
    /* User‑provided format */
    va_start(ap, fmt);
    offset += vsnprintf(buf + offset, sizeof(buf) - offset, fmt, ap);
    va_end(ap);
    /* Trailing newline */
    offset += snprintf(buf + offset, sizeof(buf) - offset, "\n");
    err_write(buf, offset);
    exit(1);
}


/** Suddenly exit the program with fatal error.
 * Unlike error(), this function does not allocates
 * any additional memory before exiting program.
 */
void            die(const char *msg)
{
    write(STDERR_FILENO, "\nfatal error: ", 14);
    perror(msg);
    exit(1);
}

#pragma GCC diagnostic pop
