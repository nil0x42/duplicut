#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "status.h"
#include "chunk.h"
#include "debug.h"

#define FACTORIAL(n)        (n * (n + 1) / 2)

#define START_TIME()        (g_status.fcopy_date)
#define FCOPY_STARTED()     (g_status.fcopy_date != 0)
#define FCOPY_TERMINATED()  (g_status.ctasks_date != 0)
#define FCOPY_DURATION()    (g_status.ctasks_date - g_status.fcopy_date)
#define TAGDUP_STARTED()    (FCOPY_TERMINATED())
#define TAGDUP_TERMINATED() (g_status.fclean_date != 0)
#define TAGDUP_DURATION()   (g_status.fclean_date - g_status.ctasks_date)
#define FCLEAN_STARTED()    (TAGDUP_TERMINATED())
#define MISSING_CTASKS()    (g_status.total_ctasks - g_status.done_ctasks)

#define MINUTE              (60)
#define HOUR                (MINUTE * 60)
#define DAY                 (HOUR * 24)

#define BUF_SIZE            (128)

struct                  status
{
    time_t              fcopy_date;
    time_t              ctasks_date;
    time_t              last_ctask_date;
    time_t              fclean_date;
    int                 total_chunks;
    int                 done_chunks;
    int                 total_ctasks;
    int                 done_ctasks;
};

static struct status    g_status = {
    .fcopy_date = 0,
    .ctasks_date = 0,
    .last_ctask_date = 0,
    .fclean_date = 0,
    .total_chunks = 0,
    .done_chunks = 0,
    .total_ctasks = 0,
    .done_ctasks = 0,
};

pthread_mutex_t         g_mutex = PTHREAD_MUTEX_INITIALIZER;


/** Update program status
 */
void                update_status(enum e_status_action action)
{
    switch (action) {
        case FCOPY_START:
            DLOG("update_status(FCOPY_START) called");
            g_status.fcopy_date = time(NULL);
            break ;
        case TAGDUP_START:
            DLOG("update_status(TAGDUP_START) called");
            g_status.ctasks_date = time(NULL);
            g_status.total_chunks = count_chunks();
            g_status.total_ctasks = FACTORIAL(g_status.total_chunks);
            break ;
        case CHUNK_DONE:
            DLOG("update_status(CHUNK_DONE) called");
            pthread_mutex_lock(&g_mutex);
            ++g_status.done_chunks;
            pthread_mutex_unlock(&g_mutex);
            break ;
        case CTASK_DONE:
            DLOG("update_status(CTASK_DONE) called");
            pthread_mutex_lock(&g_mutex);
            ++g_status.done_ctasks;
            g_status.last_ctask_date = time(NULL);
            pthread_mutex_unlock(&g_mutex);
            break ;
        case FCLEAN_START:
            DLOG("update_status(FCLEAN_START) called");
            g_status.fclean_date = time(NULL);
            break ;
    }
}


static void     repr_elapsed_time(char *buffer, unsigned long seconds)
{
    unsigned long   days, hours, minutes;

    days = seconds / DAY;
    seconds %= DAY;
    hours = seconds / HOUR;
    seconds %= HOUR;
    minutes = seconds / MINUTE;
    seconds %= MINUTE;

    snprintf(buffer, BUF_SIZE - 1, "%lu:%02lu:%02lu:%02lu",
            days, hours, minutes, seconds);
}


static void     repr_arrival_time(char *buffer, time_t arrival)
{
    time_t      current_time = time(NULL);

    if (arrival == 0)
        strncpy(buffer, "unknown", BUF_SIZE - 1);
    else
    {
        if (arrival <= current_time)
            arrival = current_time + 3;
        strftime(buffer, BUF_SIZE, "%a %b %e %T %Y", localtime(&arrival));
    }
}

static void     repr_current_task(char *buffer)
{
    if (!FCOPY_TERMINATED())
        strncpy(buffer, "step 1/3: creating output file", BUF_SIZE - 1);
    else if (!TAGDUP_TERMINATED())
        snprintf(buffer, BUF_SIZE - 1, "step 2/3: cleaning chunk %d/%d",
                g_status.done_chunks + 1, g_status.total_chunks);
    else
        strncpy(buffer, "step 3/3: removing tagged lines", BUF_SIZE - 1);
}


/** Display program status (current progression)
 */
void            display_status(void)
{
    char        elapsed_time_str[BUF_SIZE] = {0};
    char        arrival_time_str[BUF_SIZE] = {0};
    char        current_task_str[BUF_SIZE] = {0};
    double      percent_progression = 0.0;

    time_t      current_time = 0;
    time_t      elapsed_time = 0;
    time_t      arrival_time = 0;

    if (!FCOPY_STARTED())
        return ;

    current_time = time(NULL);
    elapsed_time = current_time - START_TIME();

    if (!FCOPY_TERMINATED())
    {
        percent_progression = 0.42;
    }
    else if (!TAGDUP_TERMINATED())
    {
        percent_progression = 5.0;
        if (g_status.done_chunks > 0)
        {
            double tagdup_elapsed_time = elapsed_time - FCOPY_DURATION();
            double time_per_ctask = tagdup_elapsed_time / g_status.done_ctasks;
            time_t remaining_time = time_per_ctask * MISSING_CTASKS();
            /* FCOPY_DURATION is added because it's close to FCLEAN_DURATION */
            arrival_time = current_time + remaining_time + FCOPY_DURATION();

            double percent_per_ctask = 90.0 / g_status.total_ctasks;
            percent_progression += percent_per_ctask * g_status.done_ctasks;
            double cur_ctasks_seconds = current_time - g_status.last_ctask_date;
            double ctask_progression = cur_ctasks_seconds / time_per_ctask;
            percent_progression += percent_per_ctask * ctask_progression;
        }
    }
    else
    {
        percent_progression = 95.0;

        double percent_per_second = 5.0 / (double) FCOPY_DURATION();
        time_t elapsed_fclean = current_time - g_status.fclean_date;
        percent_progression += percent_per_second * (double)elapsed_fclean;
    }

    repr_elapsed_time(elapsed_time_str, elapsed_time);
    repr_arrival_time(arrival_time_str, arrival_time);
    repr_current_task(current_task_str);
    fprintf(stderr, "time: %s %.2f%% (ETA: %s)  %s ...\n",
            elapsed_time_str,
            percent_progression,
            arrival_time_str,
            current_task_str);
}
