#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "status.h"
#include "chunk.h"
#include "debug.h"

#define TRIANGULAR(n)       (n * (n + 1) / 2)

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
    size_t              file_size;
    size_t              fcopy_bytes;
    size_t              chunk_size;
    size_t              tagdup_bytes;
    size_t              tagdup_duplicates;
    size_t              tagdup_junk_lines;
    size_t              fclean_bytes;
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
    .file_size = 0,
    .fcopy_bytes = 0,
    .chunk_size = 0,
    .tagdup_bytes = 0,
    .tagdup_duplicates = 0,
    .tagdup_junk_lines = 0,
    .fclean_bytes = 0,
};

pthread_mutex_t         g_mutex = PTHREAD_MUTEX_INITIALIZER;


/** Update program status
 */
void            update_status(enum e_status_update action)
{
    switch (action) {
        case FCOPY_START:
            DLOG1("CALL update_status(FCOPY_START)");
            g_status.fcopy_date = time(NULL);
            break ;
        case TAGDUP_START:
            DLOG1("CALL update_status(TAGDUP_START)");
            g_status.ctasks_date = time(NULL);
            g_status.total_chunks = count_chunks();
            g_status.total_ctasks = TRIANGULAR(g_status.total_chunks);
            break ;
        case CHUNK_DONE:
            DLOG1("CALL update_status(CHUNK_DONE)");
            pthread_mutex_lock(&g_mutex);
            ++g_status.done_chunks;
            pthread_mutex_unlock(&g_mutex);
            break ;
        case CTASK_DONE:
            DLOG1("CALL update_status(CTASK_DONE)");
            pthread_mutex_lock(&g_mutex);
            ++g_status.done_ctasks;
            g_status.last_ctask_date = time(NULL);
            pthread_mutex_unlock(&g_mutex);
            break ;
        case FCLEAN_START:
            DLOG1("CALL update_status(FCLEAN_START)");
            g_status.fclean_date = time(NULL);
            break ;
    }
}


void            set_status(enum e_status_set var, size_t val)
{
    switch (var) {
        case FILE_SIZE:
            DLOG1("CALL set_status(FILE_SIZE, %lu)", val);
            g_status.file_size = val;
            break ;
        case FCOPY_BYTES:
            DLOG2("CALL set_status(FCOPY_BYTES, %lu)", val);
            g_status.fcopy_bytes += val;
            break ;
        case CHUNK_SIZE:
            DLOG1("CALL set_status(CHUNK_SIZE, %lu)", val);
            g_status.chunk_size = val;
            break ;
        case TAGDUP_BYTES:
            DLOG2("CALL set_status(TAGDUP_BYTES, %lu)", val);
            pthread_mutex_lock(&g_mutex);
            g_status.tagdup_bytes += val;
            pthread_mutex_unlock(&g_mutex);
            break ;
        case TAGDUP_DUPLICATES:
            DLOG2("CALL set_status(TAGDUP_DULICATES, %lu)", val);
            pthread_mutex_lock(&g_mutex);
            g_status.tagdup_duplicates += val;
            pthread_mutex_unlock(&g_mutex);
            break ;
        case TAGDUP_JUNK_LINES:
            DLOG2("CALL set_status(TAGDUP_DULICATES, %lu)", val);
            pthread_mutex_lock(&g_mutex);
            g_status.tagdup_junk_lines += val;
            pthread_mutex_unlock(&g_mutex);
            break ;
        case FCLEAN_BYTES:
            DLOG2("CALL set_status(FCLEAN_BYTES, %lu)", val);
            g_status.fclean_bytes += val;
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


static void     repr_elapsed_time2(char *buffer, unsigned long seconds)
{
    unsigned long   days, hours, minutes;

    days = seconds / DAY;
    seconds %= DAY;
    hours = seconds / HOUR;
    seconds %= HOUR;
    minutes = seconds / MINUTE;
    seconds %= MINUTE;

    if (days)
        snprintf(buffer, BUF_SIZE - 1, "%lu days, %02lu:%02lu:%02lu",
                days, hours, minutes, seconds);
    else if (hours)
        snprintf(buffer, BUF_SIZE - 1, "%02lu:%02lu:%02lu hours",
                hours, minutes, seconds);
    else if (minutes)
        snprintf(buffer, BUF_SIZE - 1, "%02lu:%02lu minutes",
                minutes, seconds);
    else
        snprintf(buffer, BUF_SIZE - 1, "%02lu seconds",
                seconds);
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
    int cur_chunk;
    int cur_ctask;

    if (!FCOPY_TERMINATED())
    {
        if (isatty(STDERR_FILENO))
            strncpy(buffer,
                    "\e[36mstep 1/3: \e[1mcreating output file\e[0;36m ...",
                    BUF_SIZE - 1);
        else
            strncpy(buffer,
                    "step 1/3: creating output file ...",
                    BUF_SIZE - 1);
    }
    else if (!TAGDUP_TERMINATED()) {
        cur_chunk = g_status.done_chunks;
        if (cur_chunk < g_status.total_chunks)
            ++cur_chunk;
        cur_ctask = g_status.done_ctasks;
        if (cur_ctask < g_status.total_ctasks)
            ++cur_ctask;
        if (isatty(STDERR_FILENO))
            snprintf(buffer, BUF_SIZE - 1,
                    "\e[36mstep 2/3: \e[1mcleaning chunk %d/%d "
                    "\e[0;2;36m(task %d/%d)\e[0;36m ...",
                    cur_chunk, g_status.total_chunks,
                    cur_ctask, g_status.total_ctasks);
        else
            snprintf(buffer, BUF_SIZE - 1,
                    "step 2/3: cleaning chunk %d/%d "
                    "(task %d/%d) ...",
                    cur_chunk, g_status.total_chunks,
                    cur_ctask, g_status.total_ctasks);
    }
    else {
        if (isatty(STDERR_FILENO))
            strncpy(buffer,
                    "\e[36mstep 3/3: \e[1mremoving tagged lines\e[0;36m ...",
                    BUF_SIZE - 1);
        else
            strncpy(buffer,
                    "step 3/3: removing tagged lines ...",
                    BUF_SIZE - 1);
    }
}

/** Display final report
 */
void            display_report(void)
{
    char        elapsed_time_str[BUF_SIZE] = {0};
    time_t      elapsed_time = 0;

    elapsed_time = time(NULL) - START_TIME();
    repr_elapsed_time2(elapsed_time_str, elapsed_time);

    if (isatty(STDERR_FILENO))
        fprintf(stderr, "\nduplicut successfully removed "
                "\e[1m%ld\e[0m duplicates "
                "and \e[1m%ld\e[0m filtered lines "
                "in \e[1m%s\e[0m\n",
                g_status.tagdup_duplicates,
                g_status.tagdup_junk_lines,
                elapsed_time_str);
    else
        fprintf(stderr, "\nduplicut successfully removed "
                "%ld duplicates "
                "and %ld filtered lines "
                "in %s\n",
                g_status.tagdup_duplicates,
                g_status.tagdup_junk_lines,
                elapsed_time_str);
}


/** Display program status (current progression)
 */
void            display_status(void)
{
    char        elapsed_time_str[BUF_SIZE] = {0};
    char        arrival_time_str[BUF_SIZE] = {0};
    char        current_task_str[BUF_SIZE] = {0};
    time_t      current_time = 0;
    time_t      elapsed_time = 0;
    time_t      arrival_time = 0;
    double      progress = 0.0; /* 1.0 == 100% */
    double      remain_time = 0.0;

    current_time = time(NULL);
    elapsed_time = current_time - START_TIME();

    /* we need at least 1 sec execution to show status */
    if (elapsed_time == 0)
        return ;

    /* FCLEAN [3/3] --> 94% to 100% */
    if (g_status.fclean_bytes) {
        double fclean_part =
            (double)g_status.fclean_bytes / (double)g_status.file_size;
        progress = 0.94 + (fclean_part * 0.06);
        if (progress > 0.9999)
            progress = 0.9999;

        double fclean_elapsed_time = elapsed_time;
        fclean_elapsed_time -= FCOPY_DURATION() + TAGDUP_DURATION();
        if (fclean_elapsed_time >= 1) {
            remain_time = fclean_elapsed_time / fclean_part;
            remain_time -= fclean_elapsed_time;
            arrival_time = current_time + remain_time;
        }
    }
    /* TAGDUP [2/3] --> 4% to 94% */
    else if (g_status.tagdup_bytes) {
        double total_bytes = g_status.total_ctasks * g_status.chunk_size;
        double tagdup_part = (double)g_status.tagdup_bytes / total_bytes;
        progress = 0.04 + (tagdup_part * 0.90);

        double tagdup_elapsed_time = elapsed_time;
        tagdup_elapsed_time -= FCOPY_DURATION();
        if (tagdup_elapsed_time >= 1) {
            remain_time = tagdup_elapsed_time / tagdup_part;
            remain_time -= tagdup_elapsed_time;
            arrival_time = current_time + remain_time;
            /* add estimation of FCLEAN duration: */
            arrival_time += (FCOPY_DURATION() * 6) / 4;
        }
    }
    /* FCOPY [1/3] --> 0% to 4% */
    else if (g_status.fcopy_bytes) {
        progress = (double)g_status.fcopy_bytes / (double)g_status.file_size;
        progress *= 0.04;
    }
    else {
        return;
    }

    /* fallback method to display ETA */
    if (progress > 0 && arrival_time == 0) {
        remain_time = (double)elapsed_time / progress;
        remain_time -= elapsed_time;
        arrival_time = current_time + remain_time;
    }

    repr_elapsed_time(elapsed_time_str, elapsed_time);
    repr_arrival_time(arrival_time_str, arrival_time);
    repr_current_task(current_task_str);

    if (isatty(STDERR_FILENO))
        fprintf(stderr,
                "\e[0mtime: \e[1m%s \e[1;33m%5.2f%%\e[0m "
                "(ETA: \e[1m%s\e[0m)  \e[0m%s\e[0m\n",
                elapsed_time_str,
                progress * 100.0,
                arrival_time_str,
                current_task_str);
    else
        fprintf(stderr,
                "time: %s %5.2f%% "
                "(ETA: %s)  %s\n",
                elapsed_time_str,
                progress * 100.0,
                arrival_time_str,
                current_task_str);
}
