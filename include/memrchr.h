#ifndef MEMRCHR_H
# define MEMRCHR_H


/** if gnuc is available:
 * use available memrchr() function fron <string.h>
 */
// # ifdef __GNUC__
# ifdef __GNU_LIBRARY__
#  define _GNU_SOURCE
#  include <string.h>

/** else, if GNUC unavailable:
 * fallback to naive memrchr implementation
 */
# else
#  include <stddef.h>

void *memrchr(const void *s, int c, size_t n)
{
    register const char *src;
    register const char *tail;
    void *match;

    src = s;
    tail = src + n;
    match = NULL;
    while (src < tail)
    {
        if (*src == c)
            match = (void*)src;
        ++src;
    }
    return match;
}
# endif


#endif /* !MEMRCHR_H */
