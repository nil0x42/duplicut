#pragma once

/* update status */
enum    e_status_update
{
    FCOPY_START,    /* file copy starts [1/3] */
    TAGDUP_START,   /* start processing duplicate removal [2/3] */
    CHUNK_DONE,     /* a chunk is terminated */
    CTASK_DONE,     /* a ctask is terminated */
    FCLEAN_START,   /* start processing fclean (outfile cleanout) [3/3] */
};

/* update status variables */
enum    e_status_set
{
    FILE_SIZE,      /* total file size (g_infile) */
    FCOPY_BYTES,    /* currently copied bytes */
    CHUNK_SIZE,     /* chunk size */
    CLEANOUT_BYTES, /* bytes processed by cleanout_chunk() */
};

/* source file: status.c */
void                display_status(void);
void                update_status(enum e_status_update action);
void                set_status(enum e_status_set var, size_t val);
