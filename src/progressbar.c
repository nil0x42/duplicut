#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include "progressbar.h"
#include "status.h"
#include "const.h"
#include "error.h"

#define BUF_SIZE    (1024)

static struct termios           g_old_tio, g_new_tio;

static pthread_t                g_pbarthread_id = 0;
static volatile sig_atomic_t    g_pbarthread_running = 1;


/** thread worker
 * print progression status with print_status() on keypress
 */
#define SLEEPTIME_MS 250 // pbar refresh frequency
#define ITER 25 // 25 loop iterations per loop refresh
static void *progressbar_worker(void *arg)
{
    (void)arg;
    int iter = ITER;
    while (g_pbarthread_running)
    {
        if (iter == ITER) {
            display_status(0);
            iter = 0;
        }
        usleep((SLEEPTIME_MS * 1000) / ITER); // 250ms / iter
        ++iter;
    }
    display_status(1);
    return NULL;
}


/** restore terminal's line buffering & echo
 */
static void restore_termios(void)
{
    tcsetattr(STDERR_FILENO, TCSANOW, &g_old_tio);
}

/** disable terminal's line buffering & echo
 */
static void config_termios(void)
{
    if (tcgetattr(STDERR_FILENO, &g_old_tio) < 0)
        error("tcgetattr(): %s", ERRNO);

    g_new_tio = g_old_tio;
    g_new_tio.c_lflag &= ~(ICANON | ECHO);

    if (tcsetattr(STDERR_FILENO, TCSANOW, &g_new_tio) < 0)
        error("tcsetattr(): %s", ERRNO);

    atexit(restore_termios);
}


void        start_progressbar(void)
{
    if (!isatty(STDERR_FILENO))
        return ;

    /* Ignore if not in foreground */
    if (tcgetpgrp(STDERR_FILENO) != getpgrp())
        return;

    config_termios();
    if (pthread_create(
                &g_pbarthread_id, NULL,
                &progressbar_worker, NULL
                ) < 0) {
        error("cannot create watch_user_input_worker() thread: %s", ERRNO);
    }
}

void        stop_progressbar(void)
{
    if (g_pbarthread_id == 0)
        return;
    g_pbarthread_running = 0;
    pthread_join(g_pbarthread_id, NULL); // wait thread (ignored if already detached)
    restore_termios();
    g_pbarthread_id = 0;
}
