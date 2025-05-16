#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <math.h>              /* fabs() */
#include "status.h"
#include "chunk.h"
#include "debug.h"
#include "cprintferr.h"

#define TRIANGULAR(n)       (n * (n + 1) / 2)

#define START_TIME()        (g_status.fcopy_date)
#define FCOPY_TERMINATED()  (g_status.ctasks_date != 0)
#define FCOPY_DURATION()    (g_status.ctasks_date - g_status.fcopy_date)
#define TAGDUP_TERMINATED() (g_status.fclean_date != 0)
#define TAGDUP_DURATION()   (g_status.fclean_date - g_status.ctasks_date)
#define FCLEAN_TERMINATED() (g_status.fclean_bytes == g_status.file_size)

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

/*  smooth_eta ‒ Present a human-friendly, steadily changing ETA
 *
 *  Applies an exponentially weighted moving average (τ = 1 s) directly to the
 *  raw time-remaining estimate, then adds hysteresis and jump logic so the
 *  number a user sees is stable yet responsive:
 *
 *    • Minor changes (< 0.8 s or 1 %) are suppressed.
 *    • Large deviations (> 10 s and 30 %) are accepted immediately.
 *    • Between updates the displayed value is forced to drop by ≈ 1 s per
 *      real-time second so the countdown never stalls or counts up.
 *
 *  All state is kept in two static variables:
 *    displayed ― last ETA shown (seconds, double for sub-second precision)
 *    last_t    ― wall-clock time of the previous call
 *
 *  Returns a monotone-decreasing, smoothed ETA in whole seconds; returns 0 if
 *  the raw ETA is non-positive.
 */
static time_t smooth_eta(time_t eta_raw)
{
    const double TAU_SEC   = 1.0;
    const double EPS_ABS   = 0.8;
    const double EPS_REL   = 0.01;
    const double JUMP_PERC = 0.30;
    const double JUMP_ABS  = 10.0;

    static double displayed = -1.0;
    static time_t last_t    = 0;

    if (eta_raw <= 0)
        return 0;

    time_t now = time(NULL);
    if (displayed < 0) {
        displayed = eta_raw;
        last_t    = now;
        return eta_raw;
    }

    double dt   = difftime(now, last_t);
    if (dt < 1e-3)
        return (time_t)displayed;

    double alpha = dt / (TAU_SEC + dt);
    double eta   = (1.0 - alpha) * displayed + alpha * eta_raw;

    double diff  = fabs(displayed - eta);
    if (diff < EPS_ABS || diff < displayed * EPS_REL)
        eta = displayed;

    diff = fabs(displayed - (double)eta_raw);
    if (diff > JUMP_ABS && diff > displayed * JUMP_PERC)
        eta = eta_raw;

    double min_dec = dt;           /* keep countdown at ≈1 s/s */
    if (eta > displayed - min_dec)
        eta = fmax(displayed - min_dec, (double)eta_raw);

    displayed = eta;
    last_t    = now;
    return (time_t)eta;
}

/** Update program status */
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


/* Print a human-readable duration into `buf`.  
   Caller must ensure the buffer is large enough. */
static void repr_time(char *buf, time_t sec)
{
    enum { MIN = 60, HR = 60 * MIN, DAY = 24 * HR };

    if (sec >= DAY)
        sprintf(buf, "%lud %luh", sec / DAY, (sec % DAY) / HR);
    else if (sec >= HR)
        sprintf(buf, "%luh %lum", sec / HR, (sec % HR) / MIN);
    else if (sec >= MIN)
        sprintf(buf, "%lum %lus", sec / MIN, sec % MIN);
    else
        sprintf(buf, "%lus", sec);
}


static void     repr_current_task(char *buffer)
{
    if (!FCOPY_TERMINATED()) {
        strncpy(buffer,
                "\e[36m[1/3] "
                "\e[1mcreating output file"
                "\e[0;36m ...",
                BUF_SIZE - 1);
    }
    else if (!TAGDUP_TERMINATED()) {
        int cur_chunk = g_status.done_chunks
            + (g_status.done_chunks < g_status.total_chunks);
        int cur_ctask = g_status.done_ctasks
            + (g_status.done_ctasks < g_status.total_ctasks);
        snprintf(buffer, BUF_SIZE - 1,
                "\e[36m[2/3] "
                "\e[1mdedupe chunk %d/%d "
                "\e[0;2;36m(task %d/%d)"
                "\e[0;36m ...",
                cur_chunk, g_status.total_chunks,
                cur_ctask, g_status.total_ctasks
                );
    }
    else {
        strncpy(buffer,
                "\e[36m[3/3] "
                "\e[1mremoving tagged lines"
                "\e[0;36m ...",
                BUF_SIZE - 1);
    }
}

/** Display final report */
void            display_report(void)
{
    char elapsed[BUF_SIZE];

    repr_time(elapsed, time(NULL) - START_TIME());
    cprintferr(
            "duplicut successfully removed "
            "\e[1m%ld\e[0m duplicates "
            "and \e[1m%ld\e[0m filtered lines "
            "in \e[1m%s\e[0m\n",
            g_status.tagdup_duplicates,
            g_status.tagdup_junk_lines,
            elapsed
            );
}

/* ------------------------------------------------------------------------ */
/* Display current progression                                              */
/* ------------------------------------------------------------------------ */
void    display_status(int finished)
{

    char            s_t_elapsed[32];
    char            s_t_remaining[32];
    char            s_cur_task[BUF_SIZE];
    const time_t    t_elapsed = time(NULL) - START_TIME();
    time_t          t_remaining = 0;
    double          progress;

    if (!g_status.fcopy_bytes || t_elapsed < 1)
        return; // only display after 1st second
    /* ---------- [3/3] FCLEAN : 94 % → 100 % --------------------------- */
    if (g_status.fclean_bytes) {
        double step_ratio = (double) g_status.fclean_bytes / g_status.file_size;
        progress = 0.94 + (step_ratio * 0.06);
        if (progress > 0.999 && !FCLEAN_TERMINATED())
            progress = 0.999; // limit to 99.9% while not fully terminated
        double step_elapsed = t_elapsed - FCOPY_DURATION() - TAGDUP_DURATION();
        if (step_elapsed >= 1 && step_ratio > 0)
            t_remaining = step_elapsed / step_ratio - step_elapsed;
    }
    /* ---------- [2/3] TAGDUP : 4 % → 94 % ----------------------------- */
    else if (g_status.tagdup_bytes) {
        long total_bytes = g_status.total_ctasks * g_status.chunk_size;
        double step_ratio = (double)g_status.tagdup_bytes / total_bytes;
        progress = 0.04 + (step_ratio * 0.90);
        double step_elapsed = t_elapsed - FCOPY_DURATION();
        if (step_elapsed >= 1 && step_ratio > 0)
            t_remaining = (
                    (step_elapsed / step_ratio - step_elapsed) +
                    // estimate of upcomming FCLEAN duration:
                    (FCOPY_DURATION() * 1.5));
    }
    /* ---------- [1/3] FCOPY : 0 % → 4 % ------------------------------ */
    else if (g_status.fcopy_bytes) {
        double step_ratio = (double)g_status.fcopy_bytes / g_status.file_size;
        progress = 0.0 + (step_ratio * 0.04);
    }
    /* ---------- fallback ETA if still unknown ------------------------- */
    if (progress > 0 && t_remaining == 0)
        t_remaining = t_elapsed / progress - t_elapsed;
    /* ---------- render strings & print -------------------------------- */
    t_remaining = smooth_eta(t_remaining);
    repr_time(s_t_remaining, t_remaining);
    repr_time(s_t_elapsed, t_elapsed);
    repr_current_task(s_cur_task);
    cprintferr(
            "\r\e[2K\e[0mtime: \e[1m%-7s \e[1;33m%4.1f%%\e[0m "
            "(ETA: \e[1m%7s\e[0m) %s\e[0m%s",
            s_t_elapsed, progress * 100.0, s_t_remaining,
            s_cur_task, finished ? "\n" : "");
}
