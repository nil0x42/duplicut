#pragma once

#include <sys/stat.h>

struct file
{
    int         fd;
    const char  *name;
    struct stat info;
    size_t      orig_size;
    char        *addr; /* mmap address (if any) */
};

/* File object used for duplicate removal. */
extern struct file  *g_file;

/* source file: file.c */
void    init_files(void);
void    cleanup_files(void);
void    log_duplicate(char *ln, int sz);
