#include "hash.h"
#include "config.h"


/** MurmurHash3 non-cryptographic hash function wrapper.
 */
static inline void      murmur3(t_line *line, long *out)
{
    uint64_t    buf128[2];

    MurmurHash3_x64_128(LINE_ADDR(*line), LINE_SIZE(*line), SEED, buf128);
    *out = (long)(*buf128 % g_conf.hmap_size);
}


/** Hash function wrapper for t_line objects.
 */
long                    hash(t_line *line)
{
    long                key;

    murmur3(line, &key);
    return (key);
}
