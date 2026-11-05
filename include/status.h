#pragma once

#include <stdbool.h>
#include <stddef.h>

/* update status */
enum    e_status_update
{
    FCOPY_START,        /* copy input [1/5] */
    CONFIG_START,       /* prepare deduplication [2/5] */
    CHUNK_START,        /* initialize mmap-chunks tasks */
    CHUNK_DONE,         /* a chunk is terminated */
    CTASK_DONE,         /* a ctask is terminated */
    FCLEAN_START,       /* compact output [4/5] */
    FOUTPUT_START,      /* finalize direct output [5/5] */
    FWRITE_START,       /* restore output from a working file [5/5] */
};

/* update status variables */
enum    e_status_set
{
    FILE_SIZE,          /* total file size (g_infile) */
    FCOPY_BYTES,        /* bytes currently processed by FCOPY */
    CHUNK_SIZE,         /* chunk size */
    CTASK_EXTRA,        /* extra tasks for a saturated chunk */
    TAGDUP_BYTES,       /* bytes currently processed by TAGDUP */
    TAGDUP_DUPLICATES,  /* total duplicates found */
    FCLEAN_SKIPPED,     /* total duplicate and filtered lines skipped */
    FCLEAN_BYTES,       /* bytes currently processed by FCLEAN */
    FOUTPUT_SIZE,       /* bytes to restore from the working file */
    FOUTPUT_BYTES,      /* restored output bytes */
};

/* source file: status.c */
void                display_report(void);
void                display_status(int finished);
void                update_status(enum e_status_update action);
void                set_status(enum e_status_set var, size_t val);

void                partition_status(int stage, size_t total);
void                status_interactive(bool enabled);
void                status_info(const char *label, const char *fmt, ...);
