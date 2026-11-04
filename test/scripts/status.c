#include <stdlib.h>
#include <time.h>
#include "status.h"

static int g_chunk_counts;


/** Advance a deterministic clock without delaying the progress tests. */
time_t      time(time_t *result)
{
    static time_t now = 1000;

    ++now;
    if (result != NULL)
        *result = now;
    return (now);
}


int         count_chunks(void)
{
    ++g_chunk_counts;
    return (4);
}


int         main(int argc, char **argv)
{
    (void)argv;
    update_status(FCOPY_START);
    set_status(FILE_SIZE, 100);
    set_status(FCOPY_BYTES, 100);
    set_status(CHUNK_SIZE, 25);
    update_status(TAGDUP_START);
    partition_status(0, 100);
    set_status(TAGDUP_BYTES, 50);
    display_status(0);
    if (argc > 1)
    {
        partition_status(-1, 0);
        update_status(CHUNK_START);
        set_status(CTASK_EXTRA, 3);
        set_status(TAGDUP_BYTES, 50);
        display_status(0);
    }
    update_status(FCLEAN_START);
    display_status(0);
    set_status(FCLEAN_BYTES, 50);
    display_status(0);
    display_status(1);
    return (g_chunk_counts != (argc > 1));
}
