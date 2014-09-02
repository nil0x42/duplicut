#include <fcntl.h>
#include <sys/stat.h>
#include "definitions.h"
#include "exit.h"

extern t_conf       g_conf;


static int          open_file(const char *pathname, t_file *file)
{
    struct stat     info;

    if ((file->fd = open(pathname, O_RDONLY)) < 0)
        error("cannot open %s: %s", pathname, ERRNO);
    else if (fstat(file->fd, &info) < 0)
        error("cannot stat %s: %s", pathname, ERRNO);
    else if (!S_ISREG(info.st_mode))
        error("%s: not a regular file", pathname);
    else
    {
        file->name = pathname;
        file->size = info.st_size;
        return (0);
    }
    return (-1);
}


static void         close_file(t_file *file)
{
    if (close(file->fd) < 0)
        die("close()");
}


int                 chunkify_file(const char *pathname)
{
    t_file          file;
    t_chunk         *chunk;
    size_t          chunk_size;

    if (open_file(pathname, &file) < 0)
        return (-1);
    file.offset = 0;
    while (file.offset < file.size)
    {
        if (g_conf.chunk_size > file.size - file.offset)
            chunk_size = file.size - file.offset;
        else
            chunk_size = g_conf.chunk_size;
        if ((chunk = create_chunk(&file, chunk_size)) == NULL)
            abort();
        file.offset += chunk_size;
        if (chunk_size == g_conf.chunk_size)
            file.offset -= g_conf.page_size;
    }
    close_file(&file);
    return (0);
}
