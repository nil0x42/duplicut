#ifndef HASH_H
# define HASH_H

# include "line.h"

# include "murmur3.h"
# include "fasthash.h"

# define SEED (0)

long    hash(t_line *line);

#endif /* HASH_H */
