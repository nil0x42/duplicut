#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "definitions.h"


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
           "-m, --memlimit <VALUE>    Limit max used memory\n"
           "-t, --threads <NUMBER>    Max threads to use\n"
           "-h, --help                Display this help and exit\n"
           "-v, --version             Output version information and exit\n"
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


static void setopt_threads(const char *value)
{
    char        *endptr;
    int         threads;

    threads = (int) strtol(value, &endptr, 10);
    if (*endptr != '\0')
        bad_argument("threads", value, "not a number");
    else if (threads < 1)
        bad_argument("threads", value, "not a positive number");
    g_conf.threads = threads;
}


void        optparse(int argc, char **argv, int *idx)
{
    int             opt;
    struct option   options[] = {
        { "help",     no_argument,       NULL, 'h' },
        { "version",  no_argument,       NULL, 'v' },
        { "memlimit", required_argument, NULL, 'm' },
        { "cores",    required_argument, NULL, 'c' },
        { NULL,       0,                 NULL, '\0'}
    };

    while ((opt = getopt_long(argc, argv, "hvm:t:", options, NULL)) >= 0)
    {
        if (opt == 'h')
            setopt_help(optarg);
        else if (opt == 'v')
            setopt_version(optarg);
        else if (opt == 'm')
            setopt_memlimit(optarg);
        else if (opt == 't')
            setopt_threads(optarg);
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
