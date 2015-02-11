#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "optparse.h"
#include "config.h"
#include "definitions.h"
#include "bytesize.h"


/** Arguments cofiguration for getopt_long().
 */
#define OPTSTRING "o:m:t:l:hv"

static struct option    g_options[] = {
    { "output",        required_argument, NULL, 'o' },
    { "memlimit",      required_argument, NULL, 'm' },
    { "threads",       required_argument, NULL, 't' },
    { "line-max-size", required_argument, NULL, 'l' },
    { "help",          no_argument,       NULL, 'h' },
    { "version",       no_argument,       NULL, 'v' },
    { NULL,            0,                 NULL, '\0'}
};


/** Error handler in case of invalid argument.
 */
static void bad_argument(const char *name, const char *value, const char *info)
{
    fprintf(stderr, "%s: invalid argument '%s' for '--%s'\n",
            PROGNAME, value, name);
    if (info != NULL)
    {
        fprintf(stderr, "(%s)\n", info);
    }
    fprintf(stderr, "Try '%s --help' for more information.\n",
            PROGNAME);
    exit(EXIT_FAILURE);
}


/** Set input file (main argument) [MANDATORY]
 * --> The source file
 */
static void setopt_input(const char *value)
{
    ;
}


/** Set output file (-o option) [MANDATORY]
 * --> The destination file
 */
static void setopt_output(const char *value)
{
    ;
}


static void setopt_memlimit(const char *value)
{
    long    result;

    result = bytesize(value);
    if (result < 1)
    {
        bad_argument("memlimit", value, "invalid byte size representation");
    }
    else
    {
        g_conf.memlimit = result;
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
    g_conf.threads = (unsigned int) threads;
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
    g_conf.line_max_size = (unsigned int) line_max_size;
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


/** Setter for option arguments.
 * It handler all parameters which are retrieved by getopt_long().
 */
static void setopt(int opt, const char *value)
{
    int                     i;
    static struct optmap    optmap[] = {
        { 'o', setopt_output },
        { 'm', setopt_memlimit },
        { 't', setopt_threads },
        { 'l', setopt_line_max_size },
        { 'h', setopt_help },
        { 'v', setopt_version },
        { '\0', NULL }
    };

    i = 0;
    while (optmap[i].id != '\0')
    {
        if (optmap[i].id == opt)
            break ;
        ++i;
    }
    if (optmap[i].id != '\0')
        optmap[i].setopt(value);
    else
    {
        fprintf(stderr, "Try '%s --help' for more information\n", PROGNAME);
        exit(EXIT_FAILURE);
    }
}


/** Set options by filling out g_conf globale.
 * It first uses setopt() in order to set option arguments,
 * then handles main argument (input file) separately,
 * throught setopt_input().
 */
void        optparse(int argc, char **argv)
{
    int             opt;

    while ((opt = getopt_long(argc, argv, OPTSTRING, g_options, NULL)) >= 0)
    {
        setopt(opt, optarg);
    }
    if (optind == argc - 1)
    {
        setopt_input(optarg);
    }
    else
    {
        setopt_help(NULL);
    }
}
