#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <ctype.h>
#include "file.h"
#include "line.h"
#include "const.h"
#include "status.h"
#include "error.h"
#include "debug.h"
#include "bytesize.h"
#include "config.h"

#define FILE_ISSET(_f) ((_f)->fd >= 0)
#define BUF_SIZE       (0x20000) /* 128 KiB */


static struct file  g_infile;
static struct file  g_outfile;
static struct file  g_tmpfile;
static struct file  g_dupfile;


/** Ensure that two open files don't point to the same
 * file on filesystem, by comparing device & inode.
 */
static void check_files_differ(
        const struct file *infile,
        const struct file *outfile)
{
    if (infile->info.st_dev == outfile->info.st_dev
            && infile->info.st_ino == outfile->info.st_ino)
        error("%s and %s must be different", infile->name, outfile->name);
}


/** Safely close a `file`.
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


/** Destructor callback for potentially open files
 * Close input and output files, including duplicates and any temporary file.
 */
static void close_all(void)
{
    close_file(&g_infile);
    close_file(&g_outfile);
    close_file(&g_dupfile);
    close_file(&g_tmpfile);
}


/** Open (`pathname`, `flags`) -> `file`
 */
static void open_file(struct file *file, const char *pathname, int flags)
{
    if ((file->fd = open(pathname, flags, 0666)) < 0)
        error("couldn't open %s: %s", pathname, ERRNO);

    if (fstat(file->fd, &(file->info)) < 0)
        error("couldn't stat %s: %s", pathname, ERRNO);

    file->name = pathname;
}


/** Preserve stdout flags and position, using direct output when possible.
 * Return true if stdout can also serve as the working file.
 */
static bool open_stdout(void)
{
    struct file *file = &g_outfile;
    struct stat info;
    int         flags;
    int         fd;

    file->name = "/dev/stdout";
    if ((file->fd = dup(STDOUT_FILENO)) < 0)
        error("couldn't dup stdout: %s", ERRNO);
    if (fstat(file->fd, &(file->info)) < 0)
        error("couldn't stat stdout: %s", ERRNO);
    if ((flags = fcntl(file->fd, F_GETFL)) < 0)
        error("cannot get stdout flags: %s", ERRNO);
    if ((flags & O_ACCMODE) == O_RDONLY)
        error("stdout is not open for writing");

    if (!S_ISREG(file->info.st_mode) || file->info.st_size != 0
            || (flags & O_APPEND) || lseek(file->fd, 0, SEEK_CUR) != 0)
        return false;
    if ((flags & O_ACCMODE) == O_RDWR)
        return true;

    /* Reopening /dev/stdout may still yield a write-only fd on macos. */
    if ((fd = open(file->name, O_RDWR)) < 0)
        return false;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDWR || (flags & O_APPEND)
            || fstat(fd, &info) < 0
            || info.st_dev != file->info.st_dev
            || info.st_ino != file->info.st_ino
            || lseek(fd, 0, SEEK_CUR) != 0)
    {
        close(fd);
        return false;
    }
    close_file(file);
    file->fd = fd;
    return true;
}


/** Truncate regular output files after checking for aliases.
 */
static void truncate_file(struct file *file)
{
    if (S_ISREG(file->info.st_mode) && ftruncate(file->fd, 0) < 0)
        error("cannot ftruncate() %s: %s", file->name, ERRNO);
}


/** Specific opener for g_tmpfile, which also uses mkstemp.
 * This function should be called once.
 */
static void create_tmpfile(void)
{
    struct file *file = &g_tmpfile;
    static char template[] = PROGNAME "_tmpfile.XXXXXX";

    if ((file->fd = mkstemp(template)) < 0)
        error("couldn't create tmpfile '%s': %s", template, ERRNO);

    if (unlink(template) < 0)
        error("cannot unlink %s: %s", template, ERRNO);

    if (fstat(file->fd, &(file->info)) < 0)
        error("couldn't stat tmpfile '%s': %s", template, ERRNO);

    file->name = template;
}


static void buf_tolower(char *str, ssize_t size)
{
    while (size--)
    {
        str[size] = tolower(str[size]);
    }
}


static void buf_toupper(char *str, ssize_t size)
{
    while (size--)
    {
        str[size] = toupper(str[size]);
    }
}


/** Copy file from src_fd to dst_fd
 */
static void copy_file(int dst_fd, int src_fd, bool send_status)
{
    char        buffer[BUF_SIZE];
    ssize_t     nread;
    size_t      read_bytes;

#if _POSIX_C_SOURCE >= 200112L
    posix_fadvise(src_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    read_bytes = 0;
    while ((nread = read(src_fd, buffer, BUF_SIZE)) > 0)
    {
        char    *dst_ptr = buffer;
        ssize_t nwrite;

        if (g_conf.lowercase_wordlist)
            buf_tolower(buffer, nread);
        else if (g_conf.uppercase_wordlist)
            buf_toupper(buffer, nread);

        read_bytes += (size_t)nread;

        do
        {
            if ((nwrite = write(dst_fd, dst_ptr, (size_t) nread)) >= 0)
            {
                nread -= nwrite;
                dst_ptr += nwrite;
            }
            else if (errno != EINTR)
            {
                error("copy_file() -> write(): %s", ERRNO);
            }
        } while (nread > 0);

        if (send_status && read_bytes >= BUF_SIZE * 0x100) {
            set_status(FCOPY_BYTES, read_bytes);
            read_bytes = 0;
        }
    }
    if (nread != 0)
        error("copy_file() -> read(): %s", ERRNO);
}


/** Initialize input/output files and set g_file to the working file.
 * - Use a temporary file when output cannot serve as the working file.
 * - Register cleanup with atexit().
 * - Memory-map the working file for duplicate removal.
 */
void        init_files(void)
{
    struct file  *file;
    bool         use_stdout;
    bool         direct_output;

    memset(&g_infile, -1, sizeof(g_infile));
    memset(&g_outfile, -1, sizeof(g_outfile));
    memset(&g_tmpfile, -1, sizeof(g_tmpfile));
    memset(&g_dupfile, -1, sizeof(g_dupfile));

    atexit(close_all);
    use_stdout = strcmp(g_conf.outfile_name, "/dev/stdout") == 0;
    if (use_stdout)
        direct_output = open_stdout();
    open_file(&g_infile, g_conf.infile_name, O_RDONLY);
    if (!use_stdout)
    {
        open_file(&g_outfile, g_conf.outfile_name, O_CREAT | O_RDWR);
        direct_output = S_ISREG(g_outfile.info.st_mode);
    }

    if (g_conf.dupfile_name != NULL) {
        open_file(&g_dupfile, g_conf.dupfile_name, O_CREAT | O_RDWR);
    }

    check_files_differ(&g_infile, &g_outfile);
    if (FILE_ISSET(&g_dupfile)) {
        check_files_differ(&g_infile, &g_dupfile);
        check_files_differ(&g_outfile, &g_dupfile);
    }

    if (!use_stdout)
        truncate_file(&g_outfile);
    if (FILE_ISSET(&g_dupfile))
        truncate_file(&g_dupfile);

    if (direct_output)
        file = &g_outfile;
    else
    {
        create_tmpfile();
        file = &g_tmpfile;
    }

    set_status(FILE_SIZE, g_infile.info.st_size);
    copy_file(file->fd, g_infile.fd, 1);
    close_file(&g_infile);

    if (fstat(file->fd, &(file->info)) < 0)
        error("couldn't stat %s: %s", file->name, ERRNO);
    if (file->info.st_size == 0)
        exit(0);

    file->addr = mmap(NULL, (size_t) file->info.st_size,
            (PROT_READ | PROT_WRITE), MAP_SHARED, file->fd, 0);
    if (file->addr == MAP_FAILED)
        error("couldn't mmap %s: %s", file->name, ERRNO);

    file->orig_size = file->info.st_size;
    g_file = file;

    DLOG1("");
    DLOG1("---------- g_file ------------");
    DLOG1("g_file->fd:           %d", g_file->fd);
    DLOG1("g_file->name:         %s", g_file->name);
    DLOG1("g_file->addr:         %p", g_file->addr);
    DLOG1("g_file->info.st_size: %s (%ld)",
            sizerepr(g_file->info.st_size), g_file->info.st_size);
    DLOG1("------------------------------");
}


/** file destructor
 * - If tmpfile, copy it into outfile
 * - truncate file with new size.
 * - Call close_all() for cleanup.
 */
void        cleanup_files(void)
{
    struct file     *file;

    if (FILE_ISSET(&g_tmpfile))
        file = &g_tmpfile;
    else
        file = &g_outfile;

    if (munmap(file->addr, file->orig_size) < 0)
        error("cannot munmap() %s: %s", file->name, ERRNO);
    if (ftruncate(file->fd, file->info.st_size) < 0)
        error("cannot ftruncate() %s: %s", file->name, ERRNO);

    if (FILE_ISSET(&g_tmpfile))
    {
        lseek(g_tmpfile.fd, 0, SEEK_SET);
        copy_file(g_outfile.fd, g_tmpfile.fd, 0);
    }
    else if (strcmp(g_conf.outfile_name, "/dev/stdout") == 0)
    {
        if (lseek(STDOUT_FILENO, file->info.st_size, SEEK_SET) < 0)
            error("cannot seek stdout: %s", ERRNO);
    }

    close_all();
}

/** Write a newline-terminated duplicate line to the duplicates file.
 */
void        log_duplicate(char *line_ptr, int line_sz)
{
    ssize_t n;
    char buf[MAX_MAX_LINE_SIZE + 1];
    memcpy(buf, line_ptr, line_sz);
    buf[line_sz] = '\n';
    do {
        n = write(g_dupfile.fd, buf, line_sz + 1);
    } while (n == -1 && errno == EINTR);
    if (n == -1) {
        error("log_duplicate() -> write(): %s", ERRNO);
    }
}
