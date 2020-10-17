#pragma once

/* update status */
enum    e_status_update
{
    FCOPY_START,        /* file copy starts [1/3] */
    TAGDUP_START,       /* start processing duplicate removal [2/3] */
    CHUNK_DONE,         /* a chunk is terminated */
    CTASK_DONE,         /* a ctask is terminated */
    FCLEAN_START,       /* start processing fclean (outfile cleanup) [3/3] */
};

/* update status variables */
enum    e_status_set
{
    FILE_SIZE,          /* total file size (g_infile) */
    FCOPY_BYTES,        /* bytes currently processed by FCOPY */
    CHUNK_SIZE,         /* chunk size */
    TAGDUP_BYTES,       /* bytes currently processed by TAGDUP */
    TAGDUP_DUPLICATES,  /* total duplicates found */
    TAGDUP_JUNK_LINES,  /* total junk-lines (filtered) found */
    FCLEAN_BYTES,       /* bytes currently processed by FCLEAN */
};

/* source file: status.c */
void                display_report(void);
void                display_status(void);
void                update_status(enum e_status_update action);
void                set_status(enum e_status_set var, size_t val);
