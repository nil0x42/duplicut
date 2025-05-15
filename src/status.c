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

/* ------------------------------------------------------------------------ */
/* ETA smoothing helpers                                                    */
/* ------------------------------------------------------------------------ */

/*
 * Smooth ETA with a simple EWMA + hysteresis so that the progress bar
 * doesn’t jitter every 250 ms, yet still follows real trend changes.
 *   eta_raw : freshly computed ETA for this tick.
 *   return  : value to display (seconds).
 * All state is kept static inside the function — no new global needed.
 */
static time_t   smooth_eta(time_t eta_raw)
{
    /* tuning parameters (empirically good defaults) */
    const double TAU_SEC   = 1.0;     /* EWMA time constant                 */
    const double EPS_ABS   = 0.8;     /* ignore variation < 0.8 s             */
    const double EPS_REL   = 0.01;    /*   or   < 1 %                       */
    const double JUMP_PERC = 0.30;    /* jump if deviation > 30 %           */
    const double JUMP_ABS  = 10.0;    /*   and > 10 s                       */

    static double displayed = -1.0;   /* last ETA value actually shown      */
    static time_t last_t    = 0;      /* last call timestamp                */

    if (eta_raw <= 0)
        return 0;                     /* nothing to smooth */

    time_t now = time(NULL);

    /* first tick — initialise */
    if (displayed < 0) {
        displayed = (double)eta_raw;
        last_t    = now;
        return eta_raw;
    }

    /* compute time since previous update */
    double dt = difftime(now, last_t);
    if (dt < 1e-3)
        return (time_t)displayed;     /* timer glitch */

    /* EWMA directly on ETA (simpler than speed-based) */
    double alpha = dt / (TAU_SEC + dt);
    double eta_smooth = (1.0 - alpha) * displayed + alpha * (double)eta_raw;

    /* hysteresis: keep previous value if the change is tiny */
    double diff = fabs(displayed - eta_smooth);
    if (diff < EPS_ABS || diff < displayed * EPS_REL)
        eta_smooth = displayed;

    /* big deviation -> follow immediately (jump) */
    diff = fabs(displayed - eta_raw);
    if (diff > JUMP_ABS && diff > displayed * JUMP_PERC)
        eta_smooth = (double)eta_raw;

    /* guarantee a visible countdown (≈ 1 s/s) */
    double min_dec = dt;                        /* here dt ≈ 0,25 s           */
    if (eta_smooth > displayed - min_dec)
        eta_smooth = fmax(displayed - min_dec, (double)eta_raw);


    displayed = eta_smooth;
    last_t    = now;

    return (time_t)displayed;
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


/* human‑friendly elapsed‑time formatter */
static void repr_time(char *buf, unsigned long sec)
{
    unsigned long days =  sec / DAY;
    unsigned long hrs  = (sec / HOUR)   % 24;
    unsigned long mins = (sec / MINUTE)    % 60;
    unsigned long secs =  sec           % 60;

    if (days)
        snprintf(buf, BUF_SIZE, "%lud %luh", days, hrs);
    else if (hrs)
        snprintf(buf, BUF_SIZE, "%luh %lum", hrs, mins);
    else if (mins)
        snprintf(buf, BUF_SIZE, "%lum %lus", mins, secs);
    else
        snprintf(buf, BUF_SIZE, "%lus", secs);
}


static void     repr_current_task(char *buffer)
{
    int cur_chunk;
    int cur_ctask;

    if (!FCOPY_TERMINATED()) {
        strncpy(buffer,
                "\e[36m[1/3] "
                "\e[1mcreating output file"
                "\e[0;36m ...",
                BUF_SIZE - 1);
    }
    else if (!TAGDUP_TERMINATED()) {
        cur_chunk = g_status.done_chunks;
        if (cur_chunk < g_status.total_chunks)
            ++cur_chunk;
        cur_ctask = g_status.done_ctasks;
        if (cur_ctask < g_status.total_ctasks)
            ++cur_ctask;
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
    char   elapsed_time_str[BUF_SIZE]   = {0};
    char   remaining_time_str[BUF_SIZE] = {0};
    char   current_task_str[BUF_SIZE]   = {0};

    time_t now          = time(NULL);
    time_t elapsed_time = now - START_TIME();
    if (elapsed_time == 0)                 /* need at least 1 s runtime   */
        return;

    double progress     = 0.0;             /* 0.0 - 1.0                  */
    double remain_d     = 0.0;             /* remaining time (seconds)   */
    time_t remaining    = 0;               /* cast of remain_d           */

    /* ---------- [3/3] FCLEAN : 94 % → 100 % --------------------------- */
    if (g_status.fclean_bytes) {
        double part = (double)g_status.fclean_bytes /
                      (double)g_status.file_size;
        progress = 0.94 + part * 0.06;
        if (progress > 0.9999)
            progress = 0.9999;

        double elapsed_fclean = elapsed_time
                              - FCOPY_DURATION()
                              - TAGDUP_DURATION();
        if (elapsed_fclean >= 1.0 && part > 0.0) {
            remain_d   = elapsed_fclean / part - elapsed_fclean;
            remaining  = (time_t)remain_d;
        }
    }
    /* ---------- [2/3] TAGDUP : 4 % → 94 % ----------------------------- */
    else if (g_status.tagdup_bytes) {
        double total_bytes = (double)g_status.total_ctasks *
                             (double)g_status.chunk_size;
        double part = (double)g_status.tagdup_bytes / total_bytes;
        progress = 0.04 + part * 0.90;

        double elapsed_tagdup = elapsed_time - FCOPY_DURATION();
        if (elapsed_tagdup >= 1.0 && part > 0.0) {
            remain_d   = elapsed_tagdup / part - elapsed_tagdup;
            /* crude estimate of upcoming FCLEAN duration                */
            remain_d  += (FCOPY_DURATION() * 6.0) / 4.0;
            remaining  = (time_t)remain_d;
        }
    }
    /* ---------- [1/3] FCOPY : 0 % → 4 % ------------------------------ */
    else if (g_status.fcopy_bytes) {
        progress = 0.04 *
                   (double)g_status.fcopy_bytes /
                   (double)g_status.file_size;
    }
    else
        return;                            /* nothing yet to display     */

    /* ---------- fallback ETA if still unknown ------------------------- */
    if (progress > 0.0 && remaining == 0) {
        remain_d  = ((double)elapsed_time / progress) - elapsed_time;
        remaining = (time_t)remain_d;
    }

    /* ---------- smooth ETA to avoid jitter ---------------------------- */
    remaining = smooth_eta(remaining);

    /* ---------- render strings & print -------------------------------- */
    repr_time(elapsed_time_str,   (unsigned long)elapsed_time);
    repr_time(remaining_time_str, (unsigned long)remaining);
    repr_current_task(current_task_str);

    cprintferr(
            "\r\e[2K\e[0mtime: \e[1m%-7s \e[1;33m%4.1f%%\e[0m "
            "(ETA: \e[1m%7s\e[0m) %s\e[0m%s",
            elapsed_time_str,
            progress * 100.0,
            remaining_time_str,
            current_task_str,
            finished ? "\n" : ""
            );
}
