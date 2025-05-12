#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "chunk.h"

#if __SIZEOF_POINTER__ >= 8
/** On 64 bit architectures, t_line type is a tagged pointer,
 * assuming that mapped regions use addresses whose 12 MSB bits
 * are set to zero.
 * https://en.wikipedia.org/wiki/Tagged_pointer
 *
 * This way, a 64bits size_t is used to store line like this:
 *
 *  LSB                               <                                MSB
 *  ----------------------------------------------------------------------
 * |   size - 12bit   |                addr - 56bit                       |
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
# error "not x64 arch (__SIZEOF_POINTER__ != 8)"
#endif

/* source file: line.c */
bool get_next_line(
        char **p_lnptr, int *p_lnsz, t_chunk *chunk, size_t *junk_lines);
int  cmp_line(t_line hmap_item, char *line_ptr, int line_sz);
