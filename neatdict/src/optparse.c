#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "config.h"


static void bad_argument(const char *name, const char *value, const char *info)
{
    fprintf(stderr, "%s: invalid argument '%s' for '--%s'\n",
            PROGNAME, value, name);
    if (info != NULL)
        fprintf(stderr, "(%s)\n", info);
    fprintf(stderr, "Try '%s --help' for more information.\n", PROGNAME);
    exit(EXIT_FAILURE);
}


static void setopt_help(const char *value)
{
    (void)value;
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


static void setopt_version(const char *value)
{
    (void)value;
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


static void setopt_memlimit(const char *value)
{
    char        *endptr;
    char        metrics[] = "BKMGT";
    char        *ptr;
    int         c;

    g_conf.memlimit = strtol(value, &endptr, 10);
    c = toupper(*endptr);
    if (c == '\0' || c == 'O')
        c = 'B';
    ptr = strchr(metrics, c);
    if (ptr == NULL)
        bad_argument("memlimit", value, "not a byte size representation");
    c = ptr - metrics;
    while (c--)
        g_conf.memlimit *= 1024;
    if (g_conf.memlimit < 1)
        bad_argument("memlimit", value, "not a positive value");
}


static void setopt_cores(const char *value)
{
    char        *endptr;
    int         cores;

    cores = (int) strtol(value, &endptr, 10);
    if (*endptr != '\0')
        bad_argument("cores", value, "not a number");
    else if (cores < 1)
        bad_argument("cores", value, "not a positive number");
    g_conf.threads = cores;
}


static void setopt_tmpdir(const char *value)
{
    g_conf.tmpdir = value;
}


void        optparse(int argc, char **argv, int *idx)
{
    int             opt;
    struct option   options[] = {
        { "help",     no_argument,       NULL, 'h' },
        { "version",  no_argument,       NULL, 'v' },
        { "memlimit", required_argument, NULL, 'm' },
        { "cores",    required_argument, NULL, 'c' },
        { "tmpdir",   required_argument, NULL, 't' },
        { NULL,       0,                 NULL, '\0'}
    };

    while ((opt = getopt_long(argc, argv, "hvm:c:t:", options, NULL)) >= 0)
    {
        if (opt == 'h')
            setopt_help(optarg);
        else if (opt == 'v')
            setopt_version(optarg);
        else if (opt == 'm')
            setopt_memlimit(optarg);
        else if (opt == 'c')
            setopt_cores(optarg);
        else if (opt == 't')
            setopt_tmpdir(optarg);
        else
        {
            fprintf(stderr, "Try '%s --help' for more information\n", PROGNAME);
            exit(EXIT_FAILURE);
        }
    }
    if (optind == argc)
        setopt_help(NULL);
    *idx = optind;
}
