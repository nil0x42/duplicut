#include <unistd.h>
#include <fcntl.h>
#include "file.h"
#include "error.h"
#include "debug.h"


#define IS_LOADED(_f) (_f.fd >= 0)
#define USE_STDIN() (!isatty(STDIN_FILENO))

static t_file   g_infile = {
    .fd = -1,
    .name = NULL,
    .info = {0}
};


/** Load duplicut input file.
 * If `name` is NULL, stdin is used instead.
 * If both STDIN and infile are provided, STDIN takes priority.
 */
void        infile_load(const char *name)
{
    /* singleton check */
    if (IS_LOADED(g_infile))
    {
        error("infile_load(): Function called twice !");
    }

    /* get g_infile.fd */
    if (USE_STDIN())
    {
        if (name != NULL)
        {
            DLOG("Both STDIN and `infile` provided. Using STDIN.");
        }
        g_infile.fd = STDIN_FILENO;
    }
    else
    {
        if (name == NULL)
        {
            error("`infile` argument must be specified");
        }
        g_infile.fd = open(name, O_RDONLY);
        if (g_infile.fd < 0)
        {
            error("cannot open %s: %s", name, ERRNO);
        }
    }

    /* fill in g_infile from fd */
    g_infile.name = name;
    if (fstat(g_infile.fd, &g_infile.info) < 0)
    {
        
    }
}
