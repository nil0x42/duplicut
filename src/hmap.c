#include <stdlib.h>
#include "hmap.h"
#include "config.h"
#include "vars.h"
#include "error.h"


t_line      *create_hmap(void)
{
    t_line  *area;

    area = (t_line*) malloc(g_conf.hmap_size * sizeof(t_line));
    if (area == NULL)
    {
        error("cannot malloc() hash map: %s", ERRNO);
    }
    g_vars.hmap = area;
    return (area);
}


void        delete_hmap(void)
{
    if (g_vars.hmap != NULL)
    {
        free(g_vars.hmap);
    }
}
