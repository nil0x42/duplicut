#include "hash.h"
# include "config.h"


static inline void      fasthash(t_line *line, long *out)
{
    uint64_t            h;

    h = fasthash64(LINE_ADDR(*line), LINE_SIZE(*line), SEED);
    *out = (long)(h % g_conf.hmap_size);
}


static inline void      xxhash(t_line *line, long *out)
{
    unsigned long long  h;

    h = XXH64(LINE_ADDR(*line), LINE_SIZE(*line), SEED);
    *out = (long)(h % g_conf.hmap_size);
}


static inline void      murmur3(t_line *line, long *out)
{
    uint32_t            buf128[4];
    unsigned long long  h;

    MurmurHash3_x64_128(LINE_ADDR(*line), LINE_SIZE(*line), SEED, buf128);
    h = *((unsigned long long*)buf128);
    *out = (long)(h % g_conf.hmap_size);
}


long                    hash(t_line *line)
{
    long                key;

    murmur3(line, &key);
    return (key);
}
