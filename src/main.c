#include "optparse.h"
#include "config.h"
#include "file.h"
#include "dedupe.h"
#include "progressbar.h"
#include "status.h"


int             main(int argc, char **argv)
{
    optparse(argc, argv); /* setup g_conf options */

    start_progressbar();

    update_status(FCOPY_START);
    if (init_files())
    {
        update_status(CONFIG_START);
        config(); /* configure g_conf options */
        set_status(CHUNK_SIZE, g_conf.chunk_size);

        tag_duplicates();

        update_status(FCLEAN_START);
        remove_duplicates();
    }
    cleanup_files();

    stop_progressbar();

    display_report();
    return (0);
}
