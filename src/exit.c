#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "exit.h"
#include "chunk.h"
#include "vars.h"


/* static void     delete_chunk(t_chunk **chunk_ptr) */
/* { */
/*     t_chunk     *chunk; */
/*  */
/*     chunk = *chunk_ptr; */
/*     if (chunk->fd >= 0) */
/*     { */
/*         close(chunk->fd); */
/*         if (chunk->map.addr && munmap(chunk->map.addr, chunk->map.size) < 0) */
/*         { */
/*             fputs("could not unmap chunk while exiting", stderr); */
/*             perror(chunk->name); */
/*         } */
/*     } */
/*     if (*chunk->name != '\0') */
/*     { */
/*         if (unlink(chunk->name) < 0) */
/*         { */
/*             fputs("could not unlink chunk while exiting", stderr); */
/*             perror(chunk->name); */
/*         } */
/*         *chunk->name = '\0'; */
/*     } */
/*     *chunk_ptr = chunk->next; */
/*     free(chunk); */
/* } */


/** Exit the program after cleaning out
 * all created objects properly.
 */
void            exit_properly(int status)
{
    if (g_vars.hmap != NULL)
        free(g_vars.hmap);
    /* while (g_vars.chunk_list != NULL) */
    /*     delete_chunk(&g_vars.chunk_list); */
    exit(status);
}


/** Print a formatted error message and leave the program.
 * The use of printf() inside this function causes it
 * to potentially allocate memory, which could be dangerous.
 */
void            error(const char *fmt, ...)
{
    va_list     ap;

    va_start(ap, fmt);
    write(STDERR_FILENO, "\nerror: ", 8);
    vdprintf(STDERR_FILENO, fmt, ap);
    write(STDERR_FILENO, "\n", 1);
    va_end(ap);
    exit_properly(1);
}


/** Suddenly exit the program.
 * Unlike error(), this function does not allocates
 * any additionnal memory in order to leave.
 */
void            die(const char *msg)
{
    write(STDERR_FILENO, "\nfatal error: ", 14);
    perror(msg);
    exit_properly(1);
}
