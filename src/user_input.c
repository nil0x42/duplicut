#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>
#include "user_input.h"
#include "status.h"
#include "const.h"
#include "error.h"

#define BUF_SIZE    (1024)

static struct termios   g_old_tio, g_new_tio;


/** thread worker
 * print progression status with print_status() on keypress
 */
static void *watch_user_input_worker(void *arg)
{
    char        input[BUF_SIZE];

    (void)arg;
    while (1)
    {
#if DEBUG_PROGRAM_STATUS == 0
        memset(input, 0, BUF_SIZE);
        read(STDIN_FILENO, input, BUF_SIZE - 1);
#else
        usleep(100000);
#endif
        display_status();
    }
    return NULL;
}


static void restore_termios(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
}


/** disable terminal's line buffering & echo
 */
static void config_termios(void)
{
    if (tcgetattr(STDIN_FILENO, &g_old_tio) < 0)
        error("tcgetattr(): %s", ERRNO);

    g_new_tio = g_old_tio;
    g_new_tio.c_lflag &= (~ICANON & ~ECHO);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &g_new_tio) < 0)
        error("tcsetattr(): %s", ERRNO);

    atexit(restore_termios);
}


void        watch_user_input(void)
{
    pthread_t       tid;

    if (!isatty(STDIN_FILENO))
        return ;

    config_termios();
    if (pthread_create(&tid, NULL, &watch_user_input_worker, NULL) < 0)
        error("can't create watch_user_input_worker() thread: %s", ERRNO);
}
