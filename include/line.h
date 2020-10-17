#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "chunk.h"

#if __SIZEOF_POINTER__ >= 8
/** On 64 bit architectures, t_line type is a tagged pointer,
 * assuming that mapped regions use addresses whose 16 MSB bits
 * are set to zero.
 * https://en.wikipedia.org/wiki/Tagged_pointer
 *
 * This way, a 64bits size_t is used to store line like this:
 *
 *  LSB                               <                                MSB
 *  ----------------------------------------------------------------------
 * | size - 8bit |                  addr - 56bit                          |
 *  ----------------------------------------------------------------------
 *
 * TODO: May need refactoring due to Intel's 5-Level Paging (may 2017)
 */

typedef size_t  t_line;
# define LINE_ISSET(ln)         (ln != 0)
# define LINE_SIZE(ln)          ((uint8_t)ln)
# define LINE_ADDR(ln)          ((char*)(ln >> 8))
# define SET_LINE(ln, ptr, sz)  (ln = ((((uintptr_t)ptr) << 8) + (uint8_t)sz))

#else
/** Fallback to standard structure for 32 bits architectures.
 */

typedef struct  s_line
{
    char        *addr;
    int         size;
}               t_line;
# define LINE_ISSET(ln)         ((ln).addr != NULL)
# define LINE_SIZE(ln)          ((ln).size)
# define LINE_ADDR(ln)          ((ln).addr)
# define SET_LINE(ln, ptr, sz)  (ln).addr = ptr; (ln).size = sz

#endif

/* source file: line.c */
bool    get_next_line(t_line *dst, t_chunk *chunk, size_t *junk_lines);
int     cmp_line(t_line *l1, t_line *l2);
