#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <math.h>
#include "definitions.h"
#include "exit.h"
#include "debug.h"


t_conf          g_conf = { 0, 1, 0, 0, 0 };


static void     bad_argument(char *optname)
{
    fprintf(stderr, "%s: invalid argument '%s' for '--%s'\n",
            PROGNAME, optarg, optname);
    fprintf(stderr, "Try '%s --help' for more information.\n", PROGNAME);
    exit(EXIT_FAILURE);
}


static void     get_options(int argc, char **argv)
{
    int             opt;
    char            *end;
    struct option   options[] = {
        { "memory-limit", required_argument, NULL, 'm' },
        { "threads",      required_argument, NULL, 't' },
        { NULL,           0,                 NULL, '\0'}
    };

    while ((opt = getopt_long(argc, argv, "m:t:", options, NULL)) >= 0)
    {
        switch (opt)
        {
            case 'm':
                g_conf.memlimit = (size_t) strtol(optarg, &end, 10);
                if (*end != '\0')
                    bad_argument("memory-limit");
                break;
            case 't':
                g_conf.threads = (int) strtol(optarg, &end, 10);
                if (*end != '\0')
                    bad_argument("threads");
                break;
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n",
                        PROGNAME);
                exit(EXIT_FAILURE);
        }
    }
}

/** Gives the nearest prime number
 * less or equal to `n`.
 */
static long     prev_prime(long n)
{
    int             i;

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


/** Get an optimal memory repartition
 * between hasmap and chunk size according
 * to available memory.
 */
static void     distribute_memory(void)
{
    double          hmap_sz;
    double          chunk_sz;

    int             page_sz;
    double          hmap_linecost;
    double          chunk_linecost;
    double          delta;


    page_sz = sysconf(_SC_PAGESIZE);
    hmap_linecost = sizeof(t_line) * (1 / HMAP_LOAD_FACTOR);
    chunk_linecost = MEDIUM_LINE_BYTES;
    delta = chunk_linecost * (double)g_conf.threads;

    hmap_sz = hmap_linecost / (hmap_linecost + delta) * g_conf.memlimit;
    hmap_sz = prev_prime((long)hmap_sz);

    chunk_sz = (g_conf.memlimit - hmap_sz) / g_conf.threads;
    chunk_sz = ((int)chunk_sz + page_sz - 1) & ~(page_sz - 1);

    g_conf.page_size = page_sz;
    g_conf.hmap_size = (size_t) hmap_sz;
    g_conf.chunk_size = (size_t) chunk_sz;
    if (g_conf.chunk_size < (g_conf.page_size * 3))
        error("chunk_size: Can't be less than (page_size * 3)");
}


/** Fill out g_conf (program configuration)
 * taking care of user argument options.
 */
void            configure(int argc, char **argv, int *idx)
{
    size_t          tmp;

    // set memlimit
    get_options(argc, argv);
    tmp = get_available_memory();
    if (g_conf.memlimit > tmp)
        error("--memory-limit: only %ldMB of memory are available.", tmp);
    else if (g_conf.memlimit == 0)
        g_conf.memlimit = tmp;
    g_conf.memlimit *= 1024 * 1024;

    // set threads
    if (g_conf.threads < 1)
        error("--threads: cannot run with %d threads !", g_conf.threads);

    // get hmap_size and chunk_size from available memory.
    distribute_memory();
    DLOG("------------------------------");
    DLOG("g_conf.memlimit:   %ld", g_conf.memlimit);
    DLOG("g_conf.threads:    %d",  g_conf.threads);
    DLOG("g_conf.page_size:  %d",  g_conf.page_size);
    DLOG("g_conf.hmap_size:  %ld", g_conf.hmap_size);
    DLOG("g_conf.chunk_size: %ld", g_conf.chunk_size);
    DLOG("------------------------------");
    *idx = optind;
}
