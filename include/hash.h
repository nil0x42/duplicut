#ifndef HASH_H
# define HASH_H

# include "line.h"

# define XXH_INLINE_ALL
# include "xxhash.h"

# if __SIZEOF_POINTER__ == 8
#   define HASH(ln)   (XXH3_64bits(LINE_ADDR(*ln), LINE_SIZE(*ln)))
# elif __SIZEOF_POINTER__ == 4
#   define HASH(ln)   (XXH32(LINE_ADDR(*ln), LINE_SIZE(*ln), 0))
# endif

#endif /* HASH_H */
