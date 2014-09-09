#include <string.h>
#include "definitions.h"

int     linecmp(t_line *l1, t_line *l2)
{
    if (l1->size != l2->size)
        return (1);
    return (memcmp(l1->addr, l2->addr, l1->size));
}
