#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <math.h>
#include "status.h"
#include "debug.h"
#include "cprintferr.h"

#define TRIANGULAR(n)       (n * (n + 1) / 2)
#define BUF_SIZE            (256)


enum    e_phase
{
    PHASE_NONE,
    PHASE_COPY,
    PHASE_CONFIG,
    PHASE_CHECK,
    PHASE_WRITE,
    PHASE_PARTITIONS,
    PHASE_MARK,
    PHASE_CHUNKS,
    PHASE_CLEAN,
    PHASE_OUTPUT,
    PHASE_RESTORE
};

static const struct {
    const char  *name;
    int         group;
} g_phases[] = {
    {"", 0},
    {"copying input", 1},
    {"preparing deduplication", 2},
    {"checking disk-partitions", 3},
    {"writing partitions", 3},
    {"deduping partitions", 3},
    {"marking duplicates", 3},
    {"mmap-chunks", 3},
    {"compacting output", 4},
    {"finalizing output", 5},
    {"writing output", 5}
};

struct                  status
{
    enum e_phase        phase;
    double              started;
    double              phase_started;
    double              ended;
    double              mmap_base;
    int                 total_chunks;
    int                 done_chunks;
    int                 total_ctasks;
    int                 done_ctasks;
    size_t              file_size;
    size_t              fcopy_bytes;
    size_t              chunk_size;
    size_t              tagdup_bytes;
    size_t              tagdup_duplicates;
    size_t              filtered_lines;
    size_t              fclean_bytes;
    size_t              partition_total;
    size_t              output_size;
    size_t              output_bytes;
};

struct                  status_line
{
    char                elapsed[32];
    char                duration[32];
    char                eta[32];
    char                percent[16];
    char                task[BUF_SIZE];
    char                prefix[BUF_SIZE];
    char                suffix[64];
    size_t              columns;
};


static struct status    g_status = {.mmap_base = 0.04};
static pthread_mutex_t  g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  g_display_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool             g_interactive;
static bool             g_live_line;
static double           g_progress;


static double   status_time(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec + now.tv_nsec / 1e9);
}


static struct status snapshot_status(void)
{
    struct status status;

    pthread_mutex_lock(&g_mutex);
    status = g_status;
    pthread_mutex_unlock(&g_mutex);
    return (status);
}


/*  smooth_eta ‒ Present a human-friendly, steadily changing ETA
 *
 *  Applies an exponentially weighted moving average (τ = 1 s) directly to the
 *  raw time-remaining estimate, then suppresses small changes and accepts
 *  large jumps:
 *
 *    • Minor changes (< 0.8 s or 1 %) are suppressed.
 *    • Large deviations (> 10 s and 30 %) are accepted immediately.
 *    • Between updates it attempts a countdown of ≈ 1 s per second,
 *      bounded below by the raw estimate; the displayed ETA may increase.
 *
 *  All state is kept in two static variables:
 *    displayed ― last ETA shown (seconds, double for sub-second precision)
 *    last_t    ― monotonic time of the previous call
 *
 *  Returns a smoothed ETA in whole seconds. A negative input resets
 *  smoothing when the current phase changes.
 */
static time_t smooth_eta(time_t eta_raw, double now)
{
    const double TAU_SEC   = 1.0;
    const double EPS_ABS   = 0.8;
    const double EPS_REL   = 0.01;
    const double JUMP_PERC = 0.30;
    const double JUMP_ABS  = 10.0;

    static double displayed = -1.0;
    static double last_t    = 0;

    if (eta_raw < 0)
    {
        displayed = -1;
        last_t = 0;
        return (0);
    }
    if (eta_raw == 0)
        return 0;

    if (displayed < 0) {
        displayed = eta_raw;
        last_t    = now;
        return eta_raw;
    }

    double dt   = now - last_t;
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

/* Print a human-readable duration into a sufficiently large buffer. */
static void     repr_time(char *buf, time_t sec)
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


static double   status_ratio(size_t bytes, size_t total)
{
    if (!total)
        return (0);
    return (fmin(1, (double)bytes / total));
}


/** Each engine owns its part of the shared deduplication interval. */
static double   phase_progress(const struct status *status, bool complete)
{
    double ratio;

    switch (status->phase) {
        case PHASE_COPY:
            return (0.04 * (complete ? 1 : status_ratio(status->fcopy_bytes, status->file_size)));
        case PHASE_CONFIG:
            return (0.04);
        case PHASE_CHECK:
        case PHASE_WRITE:
        case PHASE_PARTITIONS:
        case PHASE_MARK:
            ratio = complete ? 1 : status_ratio(status->tagdup_bytes, status->partition_total);
            return (0.04 + 0.90 * (status->phase - PHASE_CHECK + ratio) / 4);
        case PHASE_CHUNKS:
            ratio = complete ? 1 : status_ratio(status->tagdup_bytes,
                (size_t)status->total_ctasks * status->chunk_size);
            return (status->mmap_base + (0.94 - status->mmap_base) * ratio);
        case PHASE_CLEAN:
            return (0.94 + 0.05 * (complete ? 1 : status_ratio(status->fclean_bytes, status->file_size)));
        case PHASE_OUTPUT:
        case PHASE_RESTORE:
            return (0.99 + 0.01 * (complete ? 1 : status_ratio(status->output_bytes, status->output_size)));
        default:
            return (g_progress);
    }
}


static void     repr_current_task(char *buffer, const struct status *status)
{
    if (status->phase != PHASE_CHUNKS)
        snprintf(buffer, BUF_SIZE, "%s", g_phases[status->phase].name);
    else if (!status->total_chunks)
        snprintf(buffer, BUF_SIZE, "mmap-chunks: preparing chunks");
    else
        snprintf(buffer, BUF_SIZE, "mmap-chunks: chunk %d/%d (task %d/%d)",
            status->done_chunks + (status->done_chunks < status->total_chunks),
            status->total_chunks,
            status->done_ctasks + (status->done_ctasks < status->total_ctasks),
            status->total_ctasks);
}


static double   remaining_time(const struct status *status, double now)
{
    double progress = phase_progress(status, false);
    double elapsed = now - status->phase_started;
    double ratio;

    if (status->phase == PHASE_CONFIG || !progress
            || (status->phase == PHASE_COPY && !status->file_size))
        return (-1);
    if ((status->phase == PHASE_COPY && !status->fcopy_bytes)
            || (status->phase >= PHASE_CHECK && status->phase <= PHASE_CHUNKS
                && !status->tagdup_bytes)
            || (status->phase == PHASE_CLEAN && !status->fclean_bytes))
        return (-1);
    if (status->phase == PHASE_RESTORE)
    {
        ratio = status_ratio(status->output_bytes, status->output_size);
        return (ratio > 0 ? elapsed / ratio - elapsed : -1);
    }
    if (status->phase == PHASE_OUTPUT)
        return (-1);
    return ((now - status->started) * (1 - progress) / progress);
}


/** Only animated descriptions are shortened; completed lines remain intact. */
static size_t   limit_description(char *task, size_t prefix, size_t suffix)
{
    struct winsize size;
    size_t columns = 80;
    size_t available;

    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col)
        columns = size.ws_col;
    available = columns > prefix + suffix + 1 ? columns - prefix - suffix - 1 : 0;
    if (strlen(task) > available)
    {
        if (available > 3)
            memcpy(task + available - 3, "...", 3);
        task[available] = '\0';
    }
    return (columns);
}


static void     repr_duration(char *buffer, double seconds)
{
    seconds = round(fmax(0, seconds) * 1000) / 1000;
    if (seconds < 1)
        snprintf(buffer, 32, "%.3fs", seconds);
    else
        repr_time(buffer, (time_t)seconds);
}


static void     repr_status(struct status_line *line, const struct status *status,
                    double now, bool complete)
{
    double remaining;
    int group = g_phases[status->phase].group;

    if (status->phase != PHASE_COPY || status->file_size || complete)
        snprintf(line->percent, sizeof(line->percent), "%4.1f%%", g_progress * 100);
    repr_time(line->elapsed, (time_t)fmax(0, now - status->started));
    repr_duration(line->duration, now - status->phase_started);
    repr_current_task(line->task, status);
    if (complete)
    {
        snprintf(line->prefix, sizeof(line->prefix), "time: %-7s %s [%d/5] ",
            line->elapsed, line->percent, group);
        snprintf(line->suffix, sizeof(line->suffix), " ... (done: took %s)", line->duration);
    }
    else
    {
        remaining = remaining_time(status, now);
        if (remaining >= 0)
            repr_time(line->eta, smooth_eta((time_t)remaining, now));
        snprintf(line->prefix, sizeof(line->prefix), "time: %-7s %s (ETA: %7s) [%d/5] ",
            line->elapsed, line->percent, line->eta, group);
        snprintf(line->suffix, sizeof(line->suffix), " ... (%s)", line->duration);
        line->columns = limit_description(line->task, strlen(line->prefix), strlen(line->suffix));
    }
}


static void     print_status(const struct status_line *line, int group, bool complete)
{
    const char *clear = g_live_line ? "\r\e[2K" : "";

    if (!complete && strlen(line->prefix) + strlen(line->suffix) >= line->columns)
        cprintferr("%s\e[0m%.*s\e[0m", clear, (int)line->columns - 1, line->prefix);
    else if (complete)
        cprintferr("%s\e[0mtime: \e[1m%-7s \e[1;33m%s\e[0m "
            "\e[36m[%d/5] \e[1m%s\e[0;36m ... \e[0m(\e[32mdone\e[0m: took %s)\n",
            clear, line->elapsed, line->percent, group, line->task, line->duration);
    else
        cprintferr("%s\e[0mtime: \e[1m%-7s \e[1;33m%s\e[0m "
            "(ETA: \e[1m%7s\e[0m) \e[36m[%d/5] \e[1m%s\e[0;36m ... \e[0m(%s)\e[0m",
            clear, line->elapsed, line->percent, line->eta, group, line->task, line->duration);
    g_live_line = !complete;
}


static void     render_status(const struct status *status, double now, bool complete)
{
    struct status_line line = {.eta = "--", .percent = "--.-%"};

    g_progress = fmax(g_progress, phase_progress(status, complete));
    if (!complete)
        g_progress = fmin(g_progress, 0.999);
    repr_status(&line, status, now, complete);
    print_status(&line, g_phases[status->phase].group, complete);
}


/** Freeze the previous phase before exposing the next one to the refresh thread. */
static void     change_phase(enum e_phase phase, size_t total)
{
    double now;
    struct status status;

    pthread_mutex_lock(&g_display_mutex);
    now = status_time();
    status = snapshot_status();
    if (status.phase != PHASE_NONE)
        render_status(&status, now, true);
    smooth_eta(-1, now);
    pthread_mutex_lock(&g_mutex);
    if (phase == PHASE_COPY)
        g_status.started = now;
    if (phase == PHASE_NONE)
        g_status.mmap_base = g_progress;
    g_status.phase = phase;
    g_status.phase_started = now;
    if (phase >= PHASE_CHECK && phase <= PHASE_CHUNKS)
    {
        g_status.partition_total = total;
        g_status.tagdup_bytes = 0;
    }
    pthread_mutex_unlock(&g_mutex);
    if (g_interactive && phase != PHASE_NONE)
    {
        status = snapshot_status();
        render_status(&status, now, false);
    }
    pthread_mutex_unlock(&g_display_mutex);
}


void            status_interactive(bool enabled)
{
    g_interactive = enabled;
}


/** Insert fixed information without completing or corrupting the active line. */
void            status_info(const char *label, const char *fmt, ...)
{
    char buffer[PATH_MAX + 128];
    struct status status;
    va_list ap;

    pthread_mutex_lock(&g_display_mutex);
    if (g_live_line)
        cprintferr("\r\e[2K");
    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);
    cprintferr("\e[36m%-12s\e[0m%s\n", label, buffer);
    g_live_line = false;
    status = snapshot_status();
    if (g_interactive && status.phase != PHASE_NONE)
        render_status(&status, status_time(), false);
    pthread_mutex_unlock(&g_display_mutex);
}


void            update_status(enum e_status_update action)
{
    if (action == FCOPY_START)
    {
        DLOG1("CALL update_status(FCOPY_START)");
        change_phase(PHASE_COPY, 0);
    }
    else if (action == CONFIG_START)
        change_phase(PHASE_CONFIG, 0);
    else if (action == CHUNK_START)
    {
        DLOG1("CALL update_status(CHUNK_START)");
        change_phase(PHASE_CHUNKS, 0);
    }
    else if (action == FCLEAN_START)
    {
        DLOG1("CALL update_status(FCLEAN_START)");
        change_phase(PHASE_CLEAN, 0);
    }
    else if (action == FOUTPUT_START || action == FWRITE_START)
        change_phase(action == FWRITE_START ? PHASE_RESTORE : PHASE_OUTPUT, 0);
    else
    {
        pthread_mutex_lock(&g_mutex);
        if (action == CHUNK_DONE)
            ++g_status.done_chunks;
        else if (action == CTASK_DONE)
            ++g_status.done_ctasks;
        pthread_mutex_unlock(&g_mutex);
    }
}


void            partition_status(enum e_partition_stage stage, size_t total)
{
    if (stage >= PARTITION_COUNT)
    {
        DLOG1("disk-partitions phase: %s, %zu bytes", stage == PARTITION_COUNT
                ? "counting partitions" : g_phases[PHASE_CHECK + stage].name, total);
        change_phase(PHASE_CHECK + stage, total);
    }
    else
    {
        DLOG1("disk-partitions plan declined");
        change_phase(PHASE_NONE, 0);
    }
}


void            set_status(enum e_status_set var, size_t val)
{
    pthread_mutex_lock(&g_mutex);
    switch (var) {
        case FILE_SIZE:
            g_status.file_size = val;
            break ;
        case FCOPY_BYTES:
            g_status.fcopy_bytes += val;
            break ;
        case CHUNK_SIZE:
            g_status.chunk_size = val;
            break ;
        case CHUNK_COUNT:
            g_status.total_chunks = val;
            g_status.total_ctasks = TRIANGULAR(val);
            break ;
        case CTASK_EXTRA:
            g_status.total_ctasks += val;
            break ;
        case TAGDUP_BYTES:
            g_status.tagdup_bytes += val;
            break ;
        case TAGDUP_DUPLICATES:
            g_status.tagdup_duplicates += val;
            break ;
        case FCLEAN_SKIPPED:
            g_status.filtered_lines = val - g_status.tagdup_duplicates;
            break ;
        case FCLEAN_BYTES:
            g_status.fclean_bytes += val;
            break ;
        case FOUTPUT_SIZE:
            g_status.output_size = val;
            break ;
        case FOUTPUT_BYTES:
            g_status.output_bytes += val;
            break ;
    }
    pthread_mutex_unlock(&g_mutex);
}


void            display_status(int finished)
{
    struct status status;
    double now;

    pthread_mutex_lock(&g_display_mutex);
    now = status_time();
    status = snapshot_status();
    if (status.phase != PHASE_NONE && (g_interactive || finished))
        render_status(&status, now, finished);
    if (finished)
    {
        pthread_mutex_lock(&g_mutex);
        g_status.ended = now;
        g_status.phase = PHASE_NONE;
        pthread_mutex_unlock(&g_mutex);
    }
    pthread_mutex_unlock(&g_display_mutex);
}


void            display_report(void)
{
    char elapsed[32];
    struct status status = snapshot_status();

    repr_time(elapsed, (time_t)(status.ended - status.started));
    cprintferr("duplicut successfully removed \e[1m%zu\e[0m duplicates "
        "and \e[1m%zu\e[0m filtered lines in \e[1m%s\e[0m\n",
        status.tagdup_duplicates, status.filtered_lines, elapsed);
}
