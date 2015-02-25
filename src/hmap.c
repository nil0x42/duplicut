#include <stdlib.h>
#include "hmap.h"
#include "config.h"
#include "vars.h"
#include "error.h"


static void delete_hmap(void)
{
    if (g_vars.hmap != NULL)
    {
        free(g_vars.hmap);
        g_vars.hmap = NULL;
    }
}


t_line      *create_hmap(void)
{
    t_line  *area;

    if (g_vars.hmap != NULL)
        error("create_hmap(): singleton called twice !");

    area = (t_line*) malloc(g_conf.hmap_size * sizeof(t_line));
    if (area == NULL)
    {
        error("cannot malloc() hash map: %s", ERRNO);
    }

    atexit(delete_hmap);
    g_vars.hmap = area;

    return (area);
}
