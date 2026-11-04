#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "chunk.h"

#if __SIZEOF_POINTER__ >= 8
/** On 64-bit platforms, t_line packs a line address and byte length,
 * assuming that mapped regions use addresses whose upper 12 bits
 * are set to zero.
 * https://en.wikipedia.org/wiki/Tagged_pointer
 *
 * This way, a 64-bit size_t stores a line like this:
 *
 *  LSB                               <                                MSB
 *  ----------------------------------------------------------------------
 * |   size - 12 bits  |                addr - 52 bits                      |
 *  ----------------------------------------------------------------------
 *
 * TODO: May need refactoring due to Intel's 5-Level Paging (may 2017)
 */

typedef size_t  t_line;
# define LINE_ISSET(ln)         (ln != 0)
# define LINE_SIZE(ln)          ((int)(ln & 0xFFF))
# define LINE_ADDR(ln)          ((char*)(ln >> 12))
# define MAKE_LINE(ptr, sz)     ((((uintptr_t)ptr) << 12) + (sz & 0xFFF))

#else
# error "64-bit pointers required"
#endif

/* source file: line.c */
bool get_next_line(
        char **p_lnptr, int *p_lnsz, t_chunk *chunk, size_t *junk_lines);
int  cmp_line(t_line hmap_item, char *line_ptr, int line_sz);
