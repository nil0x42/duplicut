#ifndef FILE_H
# define FILE_H

# include <sys/stat.h>

typedef struct  s_file
{
    int         fd;
    const char  *name;
    struct stat info;
}               t_file;


#endif /* !FILE_H */
