#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "file.h"
#include "const.h"
#include "error.h"

# define FILE_ISSET(_f) ((_f)->fd < 0)
# define BUF_SIZE       (4096)

static struct file  g_infile;
static struct file  g_outfile;
static struct file  g_tmpfile;


/** Safely close given struct file*
 */
static void close_file(struct file *file)
{
    if (FILE_ISSET(file))
    {
        if (close(file->fd) < 0)
            warning("cannot close %s: %s", file->name, ERRNO);
        file->fd = -1;
    }
}


/** Destructor callback for optentially open files
 * - Close infile && outfile
 * - If tmpfile:
 *   - close and delete tmpfile
 */
static void close_all(void)
{
    close_file(&g_infile);
    close_file(&g_outfile);

    if (FILE_ISSET(&g_tmpfile))
    {
        close_file(&g_tmpfile);
        if (unlink(g_tmpfile.name) < 0)
            warning("cannot unlink %s: %s", g_tmpfile.name, ERRNO);
    }
}


/** Initialize a struct file with given file name
 * It fills name, fd with open(), and info with fstat.
 */
static void open_file(struct file *file, const char *name, int flags)
{
    if ((file->fd = open(name, flags, 0666)) < 0)
        error("couldn't open %s: %s", name, ERRNO);

    if (fstat(file->fd, &(file->info)) < 0)
        error("could't stat %s: %s", name, ERRNO);

    file->name = name;
}


/** Specific opener for g_tmpfile, which also uses mkstemp.
 * This function is a singleton (sould be called once).
 */
static void create_tmpfile(void)
{
    struct file *file = &g_tmpfile;
    static char template[] = PROGNAME "_tmpfile.XXXXXX";

    if ((file->fd = mkstemp(template)) < 0)
        error("couldn't create tmpfile '%s': %s", template, ERRNO);

    if (fstat(file->fd, &(file->info)) < 0)
        error("could't stat tmpfile '%s': %s", template, ERRNO);

    file->name = template;
}


/** Copy a file in another
 */
static void file_copy(int dst_fd, int src_fd)
{
    char        buffer[BUF_SIZE];
    ssize_t     nread;

    while ((nread = read(src_fd, buffer, BUF_SIZE)) > 0)
    {
        char    *dst_ptr = buffer;
        ssize_t nwrite;

        do
        {
            if ((nwrite = write(dst_fd, dst_ptr, (size_t) nread)) >= 0)
            {
                nread -= nwrite;
                dst_ptr += nwrite;
            }
            else if (errno != EINTR)
            {
                error("file_copy() -> write(): %s", ERRNO);
            }
        } while (nread > 0);
    }
    if (nread != 0)
        error("file_copy() -> read(): %s", ERRNO);
}


/** file constructor.
 * Handle src/dst files, and return a struct file* for use by duplicut.
 * Can deal with non-regular files
 * Registers cleanup functions with atexit()
 * returned file has `addr` attribute memory maped.
 */
void        init_file(const char *infile_name, const char *outfile_name)
{
    struct file  *file;

    memset(&g_infile, -1, sizeof(g_infile));
    memset(&g_outfile, -1, sizeof(g_infile));
    memset(&g_tmpfile, -1, sizeof(g_infile));

    atexit(close_all);
    open_file(&g_infile, infile_name, O_RDONLY);
    open_file(&g_outfile, outfile_name, O_TRUNC | O_CREAT | O_EXCL | O_RDWR);

    if (S_ISREG(g_outfile.info.st_mode))
    {
        file = &g_outfile;
    }
    else
    {
        create_tmpfile();
        file = &g_tmpfile;
    }

    file_copy(file->fd, g_infile.fd);
    close_file(&g_infile);

    file->addr = mmap(NULL, (size_t) file->info.st_size,
            (PROT_READ | PROT_WRITE), MAP_PRIVATE, file->fd, 0);

    g_file = file;
}


/** file destructor
 * If tmpfile, copy it info outfile
 * Call close_all() for cleanout.
 */
void        destroy_file(void)
{
    struct file     *file;

    if (FILE_ISSET(&g_tmpfile))
    {
        file = &g_tmpfile;
        file_copy(g_tmpfile.fd, g_outfile.fd);
    }
    else
    {
        file = &g_outfile;
    }

    if (munmap(file->addr, (size_t) file->info.st_size))
        error("cannot munmap() %s: %s", file->name, ERRNO);

    close_all();
}
