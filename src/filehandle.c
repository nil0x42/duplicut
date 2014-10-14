#include "exit.h"


static void     stat_file(char *pathname)
{
    return ;
}

void            check_files(char **array, int len)
{
    int         i;
    size_t      max_filesize;

    i = 0;
    max_filesize = 0;
    while (i < len)
    {
        stat_file(array[i]);
        i++;
    }
}
