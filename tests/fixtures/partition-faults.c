#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static ssize_t (*g_pwrite)(int, const void*, size_t, off_t);
static ssize_t (*g_pread)(int, void*, size_t, off_t);
static ssize_t (*g_write)(int, const void*, size_t);
static ssize_t (*g_read)(int, void*, size_t);
static int (*g_statfs)(const char*, struct statfs*);
static int (*g_statvfs)(int, struct statvfs*);
static int (*g_mkstemp)(char*);
static FILE *(*g_fopen)(const char*, const char*);
static unsigned g_calls;
static unsigned g_reads;
static unsigned g_streams;
static unsigned g_inputs;


__attribute__((constructor))
static void     init_faults(void)
{
    g_pwrite = dlsym(RTLD_NEXT, "pwrite");
    g_pread = dlsym(RTLD_NEXT, "pread");
    g_write = dlsym(RTLD_NEXT, "write");
    g_read = dlsym(RTLD_NEXT, "read");
    g_statfs = dlsym(RTLD_NEXT, "statfs");
    g_statvfs = dlsym(RTLD_NEXT, "fstatvfs");
    g_mkstemp = dlsym(RTLD_NEXT, "mkstemp");
    g_fopen = dlsym(RTLD_NEXT, "fopen");
}


ssize_t         pwrite(int fd, const void *buffer, size_t size, off_t offset)
{
    const char  *fault = getenv("DUPLICUT_TEST_FAULT");
    unsigned    call = __atomic_fetch_add(&g_calls, 1, __ATOMIC_RELAXED);
    struct stat info;

    if (fstat(fd, &info) < 0 || info.st_nlink != 0)
        _exit(98);
    if (fault && !strcmp(fault, "write"))
    {
        errno = ENOSPC;
        return (-1);
    }
    if (fault && !strcmp(fault, "zero"))
        return (0);
    if (fault && !strcmp(fault, "partial"))
    {
        if (call % 3 == 0)
        {
            errno = EINTR;
            return (-1);
        }
        if (size > 1)
            size /= 2;
    }
    return (g_pwrite(fd, buffer, size, offset));
}


ssize_t         pread(int fd, void *buffer, size_t size, off_t offset)
{
    const char *fault = getenv("DUPLICUT_TEST_FAULT");
    unsigned call = __atomic_fetch_add(&g_reads, 1, __ATOMIC_RELAXED);
    struct stat info;

    if (fault && !strcmp(fault, "partial") && call % 3 == 0)
    {
        errno = EINTR;
        return (-1);
    }
    if (fault && (!strcmp(fault, "boundary-read")
            || (!strcmp(fault, "read") && fstat(fd, &info) == 0 && !info.st_nlink)))
        return (0);
    if (fault && !strcmp(fault, "partial") && size > 1)
        size /= 2;
    return (g_pread(fd, buffer, size, offset));
}


/** Include fortified stack-buffer reads in the same fault injection. */
ssize_t         __pread_chk(int fd, void *buffer, size_t size, off_t offset, size_t capacity)
{
    if (size > capacity)
        _exit(97);
    return (pread(fd, buffer, size, offset));
}


int             statfs(const char *path, struct statfs *info)
{
    const char *fault = getenv("DUPLICUT_TEST_FAULT");
    int result = g_statfs(path, info);

    if (result == 0)
        info->f_type = fault && !strcmp(fault, "ram") ? 0x01021994 : 0xef53;
    return (result);
}


int             fstatvfs(int fd, struct statvfs *info)
{
    const char *fault = getenv("DUPLICUT_TEST_FAULT");
    int result = g_statvfs(fd, info);

    if (result == 0 && fault && !strcmp(fault, "space"))
        info->f_bavail = 0;
    return (result);
}


int             mkstemp(char *name)
{
    const char *deny = getenv("DUPLICUT_TEST_DENY");
    const char *log = getenv("DUPLICUT_TEST_LOG");
    FILE       *stream;

    if (deny && !strncmp(name, deny, strlen(deny)))
    {
        errno = EACCES;
        return (-1);
    }
    if (log)
    {
        stream = fopen(log, "a");
        if (!stream)
            _exit(99);
        fprintf(stream, "%s\n", name);
        fclose(stream);
    }
    return (g_mkstemp(name));
}


ssize_t         write(int fd, const void *buffer, size_t size)
{
    const char  *fault = getenv("DUPLICUT_TEST_FAULT");
    struct stat info;
    unsigned    call;

    if (fault && !strncmp(fault, "stream", 6)
            && fstat(fd, &info) == 0 && S_ISREG(info.st_mode))
    {
        if (!strcmp(fault, "stream-zero"))
            return (0);
        call = __atomic_fetch_add(&g_streams, 1, __ATOMIC_RELAXED);
        if (call % 3 == 0)
        {
            errno = EINTR;
            return (-1);
        }
        if (size > 1)
            size /= 2;
    }
    return (g_write(fd, buffer, size));
}


ssize_t         read(int fd, void *buffer, size_t size)
{
    const char  *fault = getenv("DUPLICUT_TEST_FAULT");
    struct stat info;
    unsigned    call;

    if (fault && !strncmp(fault, "stream", 6)
            && fstat(fd, &info) == 0 && S_ISREG(info.st_mode))
    {
        if (!strcmp(fault, "stream-read"))
        {
            errno = EIO;
            return (-1);
        }
        call = __atomic_fetch_add(&g_inputs, 1, __ATOMIC_RELAXED);
        if (call % 3 == 0)
        {
            errno = EINTR;
            return (-1);
        }
        if (size > 1)
            size /= 2;
    }
    return (g_read(fd, buffer, size));
}


FILE            *fopen(const char *name, const char *mode)
{
    const char *fault = getenv("DUPLICUT_TEST_FAULT");
    const char *available = getenv("DUPLICUT_TEST_AVAILABLE");
    const char *log = getenv("DUPLICUT_TEST_MEM_LOG");
    FILE       *stream;
    static char memory[128];
    static char cached[] = "MemAvailable: 1073741824 kB\n";
    static char uncached[] = "MemAvailable: 65536 kB\n";

    if (!strcmp(name, "/proc/meminfo"))
    {
        if (log)
        {
            stream = g_fopen(log, "a");
            if (!stream)
                _exit(99);
            fputs("MemAvailable\n", stream);
            fclose(stream);
        }
        if (available)
        {
            snprintf(memory, sizeof(memory), "MemAvailable: %s kB\n", available);
            return (fmemopen(memory, strlen(memory), "r"));
        }
        if (fault == NULL)
            return (g_fopen(name, mode));
        if (!strcmp(fault, "cached"))
            return (fmemopen(cached, sizeof(cached) - 1, "r"));
        if (!strcmp(fault, "uncached"))
            return (fmemopen(uncached, sizeof(uncached) - 1, "r"));
    }
    return (g_fopen(name, mode));
}
