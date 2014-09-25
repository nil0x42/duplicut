#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include "definitions.h"
#include "exit.h"
#include "debug.h"


t_conf          g_conf = { 0, 1, NULL, 0, 0, 0 };


static void     display_help(void)
{
    printf("Usage: %s [OPTION]... [FILE]...\n"
           "Remove duplicates from FILE(s) without sorting them.\n"
           "\n"
           "Options:\n"
           "-m, --memlimit <MEGABYTES>  Limit max used memory\n"
           "-c, --cores <NUMBER>        Use more than one thread\n"
           "-t, --tmpdir <DIRECTORY>    Set temporary chunk container\n"
           "-h, --help                  Display this help and exit\n"
           "-v, --version               Output version information and exit\n"
           "\n", PROGNAME);
    exit(EXIT_SUCCESS);
}

static void     display_version(void)
{

    printf("%s: (neatcrack suite) %s <%s>\n"
           "License GPLv3+: GNU GPL version 3"
           " or later <http://gnu.org/licenses/gpl.html>\n"
           "\n"
           "This is free software; you are free to"
           " change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n",
           PROGNAME, PROJECT_VERSION, PROJECT_URL);
    exit(EXIT_SUCCESS);
}


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
        { "memlimit", required_argument, NULL, 'm' },
        { "cores",    required_argument, NULL, 'c' },
        { "tmpdir",   required_argument, NULL, 't' },
        { "help",     no_argument,       NULL, 'h' },
        { "version",  no_argument,       NULL, 'v' },
        { NULL,       0,                 NULL, '\0'}
    };

    while ((opt = getopt_long(argc, argv, "m:c:t:hv", options, NULL)) >= 0)
    {
        switch (opt)
        {
            case 'm':
                g_conf.memlimit = (size_t) strtol(optarg, &end, 10);
                if (*end != '\0')
                    bad_argument("memlimit");
                break;
            case 'c':
                g_conf.threads = (int) strtol(optarg, &end, 10);
                if (*end != '\0')
                    bad_argument("cores");
                break;
            case 't':
                g_conf.tmpdir = optarg;
                break;
            case 'h':
                display_help();
                break;
            case 'v':
                display_version();
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
    /* chunk_sz = ((int)chunk_sz + page_sz - 1) & ~(page_sz - 1); */
    chunk_sz = (int)(chunk_sz) - ((int)chunk_sz % page_sz);

    g_conf.page_size = page_sz;
    g_conf.hmap_size = (size_t) hmap_sz;
    g_conf.chunk_size = (size_t) chunk_sz;
    if (g_conf.chunk_size < (size_t)(g_conf.page_size * 3))
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
        error("--memlimit: only %ldMB of memory are available.", tmp);
    else if (g_conf.memlimit == 0)
        g_conf.memlimit = tmp;
    /* g_conf.memlimit *= 1024 * 1024; */
    g_conf.memlimit *= 1024; // to kilobytes for testing

    // set threads
    if (g_conf.threads < 1)
        error("--cores: cannot run with %d threads !", g_conf.threads);

    // set tmpdir
    if (g_conf.tmpdir == NULL)
    {
        if ((g_conf.tmpdir = getenv("TMPDIR")) == NULL)
            g_conf.tmpdir = DEFAULT_TMPDIR;
    }
    if (g_conf.tmpdir != NULL)
        if (strlen(g_conf.tmpdir) + strlen(CHUNK_FILENAME) >= CHUNK_PATHSIZE)
            error("--tmpdir: '%s': path size is too long");

    // get hmap_size and chunk_size from available memory.
    distribute_memory();
    DLOG("------------------------------");
    DLOG("g_conf.memlimit:   %ld", g_conf.memlimit);
    DLOG("g_conf.threads:    %d",  g_conf.threads);
    DLOG("g_conf.tmpdir:     %s",  g_conf.tmpdir);
    DLOG("g_conf.page_size:  %d",  g_conf.page_size);
    DLOG("g_conf.hmap_size:  %ld", g_conf.hmap_size);
    DLOG("g_conf.chunk_size: %ld", g_conf.chunk_size);
    DLOG("------------------------------");
    *idx = optind;
}
