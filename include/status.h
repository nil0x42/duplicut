#pragma once

enum    e_status_action
{
    FCOPY_START, /* file copy starts */
    TAGDUP_START, /* start processing duplicate removal */
    CHUNK_DONE, /* a chunk is terminated */
    CTASK_DONE, /* a ctask is terminated */
    FCLEAN_START, /* start processing fclean (outfile cleanout) */
};

/* source file: status.c */
void                display_status(void);
void                update_status(enum e_status_action action);
void                update_status_fcopy(double portion_read);
