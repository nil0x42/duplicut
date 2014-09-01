#include <stdio.h>
#include <math.h>

#define HMAP_LOAD_FACTOR  (0.5)
#define MEDIUM_LINE_BYTES (5)
#define AVAILABLE_MEMORY  (0x100000000) // 4GB

#define PAGE_SIZE         (4096)

#define PAGE_ROUND(sz) ((sz + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))

typedef struct      s_line
{
    char            *addr;
    unsigned int    size;
}                   t_line;


long        prev_prime(long n)
{
    int     i;

    n = (n - 1) | 1;
    while (n > 0)
    {
        i = 3;
        while (i && i <= sqrt(n))
        {
            if (n % i == 0)
                i = 0;
            else
                i += 2;
        }
        if (i)
            return (n);
        n -= 2;
    }
    return (n);
}


int         main()
{
    long    hmap_size;
    long    chunk_size;

    float   running_threads = 1;
    float   hmap_linecost = sizeof(t_line) * (1 / HMAP_LOAD_FACTOR);
    float   chunk_linecost = MEDIUM_LINE_BYTES;

    float   delta = chunk_linecost * running_threads;
    printf("delta: %d\n", delta);

    hmap_size = hmap_linecost / (hmap_linecost + delta) * AVAILABLE_MEMORY;
    hmap_size = prev_prime(hmap_linecost);
    printf("hmap_size: %ld\n", hmap_size);

    chunk_size = (AVAILABLE_MEMORY - hmap_size) / running_threads;
    chunk_size = PAGE_ROUND((long)chunk_size);
    printf("chunk_size: %ld\n", chunk_size);

    return (0);
}
