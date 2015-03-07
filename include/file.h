#ifndef FILE_H
# define FILE_H

# include <sys/stat.h>

typedef struct  s_file
{
    int         fd;
    const char  *name;
    struct stat info;
    char        *addr; /* mmap address (if any) */
}               t_file;


extern t_file   *g_file;


/* source file: file.c */
t_file  *file_init(const char *infile_name, const char *outfile_name);
void    file_destroy(void);

#endif /* !FILE_H */
