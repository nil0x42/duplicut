#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include "optparse.h"
#include "config.h"
#include "const.h"
#include "bytesize.h"
#include "error.h"
#include "debug.h"


/** Arguments cofiguration for getopt_long().
 */
#define OPTSTRING "o:t:m:l:pcChv"

static struct option    g_options[] = {
    { "outfile",       required_argument, NULL, 'o' },
    { "threads",       required_argument, NULL, 't' },
    { "memlimit",      required_argument, NULL, 'm' },
    { "line-max-size", required_argument, NULL, 'l' },
    { "printable",     no_argument,       NULL, 'p' },
    { "lowercase",     no_argument,       NULL, 'c' },
    { "uppercase",     no_argument,       NULL, 'C' },
    { "help",          no_argument,       NULL, 'h' },
    { "version",       no_argument,       NULL, 'v' },
    { NULL,            0,                 NULL, '\0'},
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
static void setopt_infile(const char *value)
{
    g_conf.infile_name = value;
}


/** Set output file (-o option) [MANDATORY]
 * --> The destination file
 */
static void setopt_outfile(const char *value)
{
    g_conf.outfile_name = value;
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


static void setopt_memlimit(const char *value)
{
    long        memlimit;

    memlimit = bytesize(value);

    if (memlimit < 0)
        bad_argument("memlimit", value, "not a positive value");

    g_conf.memlimit = memlimit;
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


static void setopt_printable(const char *value)
{
    (void)value;

    g_conf.filter_printable = 1;
}


static void setopt_lowercase(const char *value)
{
    (void)value;

    g_conf.lowercase_wordlist = 1;
}


static void setopt_uppercase(const char *value)
{
    (void)value;

    g_conf.uppercase_wordlist = 1;
}


static void setopt_help(const char *value)
{
    (void)value;
    printf("Usage: %s [OPTION]... [INFILE] -o [OUTFILE]\n"
           "Remove duplicate lines from INFILE without changing order.\n"
           "\n"
           "Options:\n"
           "-o, --outfile <FILE>       Write result to <FILE>\n"
           "-t, --threads <NUM>        Max threads to use (default max)\n"
           "-m, --memlimit <VALUE>     Limit max used memory (default max)\n"
           "-l, --line-max-size <NUM>  Max line size (default %d)\n"
           "-p, --printable            Filter ascii printable lines\n"
           "-c, --lowercase            Convert wordlist to lowercase\n"
           "-C, --uppercase            Convert wordlist to uppercase\n"
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
        { 'o', setopt_outfile },
        { 't', setopt_threads },
        { 'm', setopt_memlimit },
        { 'l', setopt_line_max_size },
        { 'p', setopt_printable },
        { 'c', setopt_lowercase },
        { 'C', setopt_uppercase },
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
    {
        optmap[i].setopt(value);
    }
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
        setopt(opt, optarg);

    /* STDIN can be used as infile (priority to --infile arg if exists) */
    if (optind == argc - 1)
    {
        DLOG1("using %s as input file", argv[argc - 1]);
        setopt_infile(argv[argc - 1]);
    }
    else if (optind == argc && !isatty(STDIN_FILENO))
    {
        DLOG1("using STDIN as input file");
        setopt_infile("/dev/stdin");
    }
    else
        setopt_help(NULL);

    if (g_conf.lowercase_wordlist && g_conf.uppercase_wordlist)
        error("cannot use '--lowercase' and '--uppercase' together");

    if (g_conf.outfile_name == NULL)
        error("mandatory argument: --outfile");
}
