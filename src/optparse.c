#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "definitions.h"
#include "bytesize.h"


static void bad_argument(const char *name, const char *value, const char *info)
{
    fprintf(stderr, "%s: invalid argument '%s' for '--%s'\n",
            PROGNAME, value, name);
    if (info != NULL)
    {
        fprintf(stderr, "(%s)\n", info);
    }
    fprintf(stderr, "Try '%s --help' for more information.\n", PROGNAME);
    exit(EXIT_FAILURE);
}


static void setopt_help(const char *value)
{
    (void)value;
    printf("Usage: %s [OPTION]... [INFILE] -o [OUTFILE]\n"
           "Remove duplicate lines from INFILE without sorting.\n"
           "\n"
           "Options:\n"
           "-o, --output <FILE>        Write result to <FILE>\n"
           "-m, --memlimit <VALUE>     Limit max used memory (default max)\n"
           "-t, --threads <NUM>        Max threads to use (default max)\n"
           "-l, --line-max-size <NUM>  Max line size (default %d)\n"
           "-h, --help                 Display this help and exit\n"
           "-v, --version              Output version information and exit\n"
           "\n"
           "Example: %s wordlist.txt -o new-wordlist.txt\n"
           "\n", PROGNAME, DEFAULT_LINE_MAX_SIZE, PROGNAME);
    exit(EXIT_SUCCESS);
}


static void setopt_version(const char *value)
{
    (void)value;
    printf("%s %s <%s>\n"
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
    g_conf.memlimit = bytesize(value);
    if (g_conf.memlimit < 1)
    {
        bad_argument("memlimit", value, "invalid byte size representation");
    }
}


static void setopt_threads(const char *value)
{
    char        *endptr;
    int         threads;

    threads = (int) strtol(value, &endptr, 10);
    if (*endptr != '\0')
    {
        bad_argument("threads", value, "not a number");
    }
    else if (threads < 1)
    {
        bad_argument("threads", value, "not a positive number");
    }
    g_conf.threads = threads;
}


static void setopt_line_max_size(const char *value)
{
    char        *endptr;
    int         line_max_size;

    line_max_size = (int) strtol(value, &endptr, 10);
    if (*endptr != '\0')
    {
        bad_argument("line_max_size", value, "not a number");
    }
    else if (line_max_size < 1)
    {
        bad_argument("line_max_size", value, "not a positive number");
    }
    else if (line_max_size > 255)
    {
        bad_argument("line_max_size", value, "max value is 255");
    }
    g_conf.line_max_size = line_max_size;
}


static void setopt_output(const char *value)
{
    ;
}


void        optparse(int argc, char **argv, int *idx)
{
    int             opt;
    struct option   options[] = {
        { "help",          no_argument,       NULL, 'h' },
        { "version",       no_argument,       NULL, 'v' },
        { "memlimit",      required_argument, NULL, 'm' },
        { "threads",       required_argument, NULL, 't' },
        { "line-max-size", required_argument, NULL, 'l' },
        { "output",        required_argument, NULL, 'o' },
        { NULL,            0,                 NULL, '\0'}
    };

    while ((opt = getopt_long(argc, argv, "o:m:t:l:hv", options, NULL)) >= 0)
    {
        if (opt == 'o')
        {
            setopt_output(optarg);
        }
        else if (opt == 'm')
        {
            setopt_memlimit(optarg);
        }
        else if (opt == 't')
        {
            setopt_threads(optarg);
        }
        else if (opt == 'l')
        {
            setopt_line_max_size(optarg);
        }
        else if (opt == 'h')
        {
            setopt_help(optarg);
        }
        else if (opt == 'v')
        {
            setopt_version(optarg);
        }
        else
        {
            fprintf(stderr, "Try '%s --help' for more information\n", PROGNAME);
            exit(EXIT_FAILURE);
        }
    }
    if (optind == argc)
    {
        setopt_help(NULL);
    }
    *idx = optind;
}
