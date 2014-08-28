#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/** Exit the program after cleaning out
 * all created objects properly.
 */
void        exit_properly(int status)
{
    exit(status);
}

/** Print a formatted error message and leave the program.
 * The use of printf() inside this function causes it
 * to potentially allocate memory, which could be dangerous.
 */
void        error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    write(STDERR_FILENO, "error: ", 7);
    vdprintf(STDERR_FILENO, fmt, ap);
    write(STDERR_FILENO, "\n", 1);
    va_end(ap);
    exit_properly(1);
}

/** Suddenly exit the program.
 * Unlike error(), this function does not allocates
 * any additionnal memory in order to leave.
 */
void        die(const char *msg)
{
    write(STDERR_FILENO, "fatal error: ", 13);
    perror(msg);
    exit_properly(1);
}
