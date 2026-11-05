#include <stdlib.h>
#include <time.h>
#include "status.h"

static int g_chunk_counts;
static double g_now = 1000;


/** Control phase durations independently of the refresh interval. */
int         clock_gettime(clockid_t clock, struct timespec *result)
{
    (void)clock;
    result->tv_sec = (time_t)g_now;
    result->tv_nsec = (long)((g_now - result->tv_sec) * 1e9);
    return (0);
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
    g_now += 0.123;
    update_status(CONFIG_START);
    g_now += 0.456;
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
    else
    {
        g_now += 1.234;
        partition_status(1, 100);
        set_status(TAGDUP_BYTES, 100);
        g_now += 60;
        partition_status(2, 100);
        set_status(TAGDUP_BYTES, 100);
        g_now += 3600;
        partition_status(3, 100);
        set_status(TAGDUP_BYTES, 100);
    }
    g_now += 60;
    update_status(FCLEAN_START);
    display_status(0);
    set_status(FCLEAN_BYTES, 50);
    display_status(0);
    set_status(FCLEAN_BYTES, 50);
    g_now += 3600;
    update_status(FOUTPUT_START);
    display_status(1);
    return (g_chunk_counts != (argc > 1));
}
