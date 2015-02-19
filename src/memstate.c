#include <unistd.h>
#include "memstate.h"
#include "meminfo.h"
#include "error.h"


static int      get_page_size(void)
{
    int         page_size;

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0)
        error("sysconf(_SC_PAGESIZE): %s", ERRNO);

    return (page_size);
}


static long     get_mem_available(void)
{
    long        mem_available;

    mem_available = meminfo(MEMAVAILABLE);
    if (mem_available < 0)
        error("meminfo(MEMAVAILABLE): Cannot determine available memory");

    return (mem_available);
}


/** Update current memory state.
 * Unlike `init_memstate()`, this function only refreshes
 * ther values that may have changed.
 * For example, it does not changes `page_size`, which
 * is a static value.
 */
void        update_memstate(struct memstate *m)
{
    m->mem_available = get_mem_available();
}


/** Initialize current memory state.
 */
void        init_memstate(struct memstate *m)
{
    m->page_size = get_page_size();
    m->mem_available = get_mem_available();
}
