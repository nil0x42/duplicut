#ifndef VARS_H
# define VARS_H

# include "chunk.h"

typedef struct      s_vars
{
    t_chunk         *chunk_list;
    int             num_chunks;
    int             treated_chunks;
}                   t_vars;

extern t_vars       g_vars;

void                print_remaining_time(void);

#endif /* VARS_H */
