#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <ctype.h>
#include <sys/statvfs.h>
#ifdef __APPLE__
# include <sys/mount.h>
#else
# include <sys/vfs.h>
# include <linux/magic.h>
#endif
#include "file.h"
#include "line.h"
#include "const.h"
#include "status.h"
#include "error.h"
#include "debug.h"
#include "bytesize.h"
#include "config.h"
#include "meminfo.h"

#define FILE_ISSET(_f) ((_f)->fd >= 0)
#define BUF_SIZE       (0x20000) /* 128 KiB */


struct file *g_file;


static struct file  g_infile;
static struct file  g_outfile;
static struct file  g_tmpfile;
static struct file  g_dupfile;

static pthread_mutex_t g_dupfile_mutex = PTHREAD_MUTEX_INITIALIZER;


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


/** True for filesystems whose file contents occupy memory. */
static bool is_ramfs(const struct statfs *fs)
{
#ifdef __APPLE__
    return (!strcmp(fs->f_fstypename, "tmpfs") || !strcmp(fs->f_fstypename, "mfs"));
#else
    return (fs->f_type == TMPFS_MAGIC || fs->f_type == RAMFS_MAGIC);
#endif
}


/** Large RAM-backed outputs receive only the final result, not the work file. */
static bool can_map_output(const struct file *file)
{
    struct statfs fs;
    long long     budget;

    if (fstatfs(file->fd, &fs) < 0)
        error("cannot stat output filesystem: %s", ERRNO);
    if (!is_ramfs(&fs))
        return (true);
    budget = meminfo(MEMAVAILABLE);
    if (budget < 0)
        error("cannot determine available memory for RAM-backed output");
    if (budget > g_conf.memlimit)
        budget = g_conf.memlimit;
    if (S_ISREG(g_infile.info.st_mode) && g_infile.info.st_size <= budget / 2)
        return (true);
    DLOG1("RAM-backed output: using a disk-backed working file");
    return (false);
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
    if (!can_map_output(file))
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


/** Try a disk-backed temporary file beside a named file or in a directory. */
static int      try_tmpfile(const char *path, bool directory, size_t size)
{
    const char      pattern[] = "/" PROGNAME "_tmpfile.XXXXXX";
    char            *resolved;
    char            *name;
    char            *slash;
    struct stat     info;
    struct statfs   fs;
    struct statvfs  space;
    int             fd;

    if ((resolved = realpath(path, NULL)) == NULL)
        return (-1);
    if (!directory)
    {
        if (stat(resolved, &info) < 0 || !S_ISREG(info.st_mode)
                || strncmp(resolved, "/dev/", 5) == 0)
        {
            free(resolved);
            return (-1);
        }
        slash = strrchr(resolved, '/');
        slash[1] = '\0';
    }
    if (statfs(resolved, &fs) < 0)
    {
        free(resolved);
        return (-1);
    }
    if (is_ramfs(&fs))
    {
        free(resolved);
        errno = ENOSPC;
        return (-1);
    }
    name = malloc(strlen(resolved) + sizeof(pattern));
    if (name == NULL)
    {
        free(resolved);
        errno = ENOMEM;
        return (-1);
    }
    sprintf(name, "%s%s", resolved, pattern);
    free(resolved);
    fd = mkstemp(name);
    if (fd >= 0)
    {
        if (unlink(name) < 0)
            error("cannot unlink %s: %s", name, ERRNO);
        if (fstatvfs(fd, &space) < 0 || !space.f_frsize || !space.f_bavail
                || size / space.f_frsize + (size % space.f_frsize != 0)
                    > space.f_bavail)
        {
            close(fd);
            fd = -1;
            errno = ENOSPC;
        }
    }
    if (fd >= 0)
    {
        DLOG1("temporary file: %s", name);
        slash = strrchr(name, '/');
        while (slash > name && slash[-1] == '/')
            --slash;
        status_info("temp dir:", "%.*s", slash == name ? 1 : (int)(slash - name), name);
    }
    free(name);
    return (fd);
}


/** Prefer the source volume, then the output volume, then the current volume. */
int             open_tmpfile(size_t size)
{
    int fd;

    fd = try_tmpfile(g_conf.infile_name, false, size);
    if (fd < 0)
        fd = try_tmpfile(g_conf.outfile_name, false, size);
    if (fd < 0)
        fd = try_tmpfile(".", true, size);
    return (fd);
}


/** Open an anonymous working file on an available source or output volume. */
static void create_tmpfile(void)
{
    struct file *file = &g_tmpfile;

    file->fd = open_tmpfile(g_infile.info.st_size);
    if (file->fd < 0)
        error("cannot create temporary file on a disk volume: %s", ERRNO);
    if (fstat(file->fd, &(file->info)) < 0)
        error("cannot stat temporary file: %s", ERRNO);
    file->name = "temporary file";
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
static void copy_file(int dst_fd, int src_fd, enum e_status_set progress)
{
    char        buffer[BUF_SIZE];
    ssize_t     nread;
    size_t      read_bytes;

#if _POSIX_C_SOURCE >= 200112L
    posix_fadvise(src_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    read_bytes = 0;
    while (true)
    {
        char    *dst_ptr = buffer;
        ssize_t nwrite;

        do {
            nread = read(src_fd, buffer, BUF_SIZE);
        } while (nread < 0 && errno == EINTR);
        if (nread <= 0)
            break;
        if (g_conf.lowercase_wordlist)
            buf_tolower(buffer, nread);
        else if (g_conf.uppercase_wordlist)
            buf_toupper(buffer, nread);

        read_bytes += (size_t)nread;

        do
        {
            if ((nwrite = write(dst_fd, dst_ptr, (size_t) nread)) > 0)
            {
                nread -= nwrite;
                dst_ptr += nwrite;
            }
            else if (nwrite == 0 || errno != EINTR)
            {
                error("copy_file() -> write(): %s", nwrite < 0 ? ERRNO : "short write");
            }
        } while (nread > 0);

        if (read_bytes >= BUF_SIZE * 0x100) {
            set_status(progress, read_bytes);
            read_bytes = 0;
        }
    }
    if (nread != 0)
        error("copy_file() -> read(): %s", ERRNO);
    set_status(progress, read_bytes);
}


/** Initialize input/output files and set g_file to the working file.
 * - Use a temporary file when output cannot serve as the working file.
 * - Register cleanup with atexit().
 * - Memory-map the working file for duplicate removal.
 * Return false for an empty input.
 */
bool        init_files(void)
{
    struct file  *file;
    bool         use_stdout;
    bool         direct_output;

    memset(&g_infile, -1, sizeof(g_infile));
    memset(&g_outfile, -1, sizeof(g_outfile));
    memset(&g_tmpfile, -1, sizeof(g_tmpfile));
    memset(&g_dupfile, -1, sizeof(g_dupfile));

    atexit(close_all);
    open_file(&g_infile, g_conf.infile_name, O_RDONLY);
    set_status(FILE_SIZE, g_infile.info.st_size);
    use_stdout = strcmp(g_conf.outfile_name, "/dev/stdout") == 0;
    if (use_stdout)
        direct_output = open_stdout();
    if (!use_stdout)
    {
        open_file(&g_outfile, g_conf.outfile_name, O_CREAT | O_RDWR);
        direct_output = S_ISREG(g_outfile.info.st_mode) && can_map_output(&g_outfile);
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

    status_info("input:", "%s (%s)", g_conf.infile_name,
        S_ISREG(g_infile.info.st_mode) ? sizerepr(g_infile.info.st_size) : "size unknown");
    status_info("output:", "%s (%s)", g_conf.outfile_name,
        direct_output ? "direct" : "temporary work file");
    if (g_conf.dupfile_name != NULL)
        status_info("duplicates:", "%s", g_conf.dupfile_name);
    if (direct_output)
        file = &g_outfile;
    else
    {
        create_tmpfile();
        file = &g_tmpfile;
    }

    copy_file(file->fd, g_infile.fd, FCOPY_BYTES);
    close_file(&g_infile);

    if (fstat(file->fd, &(file->info)) < 0)
        error("couldn't stat %s: %s", file->name, ERRNO);
    set_status(FILE_SIZE, file->info.st_size);
    file->orig_size = file->info.st_size;
    file->addr = NULL;
    g_file = file;
    if (!S_ISREG(g_infile.info.st_mode))
        status_info("input size:", "%s", sizerepr(file->info.st_size));
    if (file->info.st_size == 0)
    {
        DLOG1("empty input: nothing to deduplicate");
        return (false);
    }

    file->addr = mmap(NULL, (size_t) file->info.st_size,
            (PROT_READ | PROT_WRITE), MAP_SHARED, file->fd, 0);
    if (file->addr == MAP_FAILED)
        error("couldn't mmap %s: %s", file->name, ERRNO);

    DLOG1("");
    DLOG1("---------- g_file ------------");
    DLOG1("g_file->fd:           %d", g_file->fd);
    DLOG1("g_file->name:         %s", g_file->name);
    DLOG1("g_file->addr:         %p", g_file->addr);
    DLOG1("g_file->info.st_size: %s (%ld)",
            sizerepr(g_file->info.st_size), g_file->info.st_size);
    DLOG1("------------------------------");
    return (true);
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

    set_status(FOUTPUT_SIZE, FILE_ISSET(&g_tmpfile) ? file->info.st_size : 0);
    update_status(FILE_ISSET(&g_tmpfile) ? FWRITE_START : FOUTPUT_START);
    if (file->orig_size && munmap(file->addr, file->orig_size) < 0)
        error("cannot munmap() %s: %s", file->name, ERRNO);
    if (ftruncate(file->fd, file->info.st_size) < 0)
        error("cannot ftruncate() %s: %s", file->name, ERRNO);

    if (FILE_ISSET(&g_tmpfile))
    {
        lseek(g_tmpfile.fd, 0, SEEK_SET);
        copy_file(g_outfile.fd, g_tmpfile.fd, FOUTPUT_BYTES);
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
    size_t  written = 0;
    size_t  size = line_sz + 1;
    char    buf[MAX_MAX_LINE_SIZE + 1];

    memcpy(buf, line_ptr, line_sz);
    buf[line_sz] = '\n';
    pthread_mutex_lock(&g_dupfile_mutex);
    while (written < size)
    {
        n = write(g_dupfile.fd, buf + written, size - written);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            error("log_duplicate() -> write(): %s", n < 0 ? ERRNO : "short write");
        written += n;
    }
    pthread_mutex_unlock(&g_dupfile_mutex);
}
