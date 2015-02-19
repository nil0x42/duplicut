#ifndef MEMSTATE_H
# define MEMSTATE_H


struct              memstate
{
    int             page_size;
    long            mem_available;
};


void        init_memstate(struct memstate *m);
void        update_memstate(struct memstate *m);


#endif /* !MEMSTATE_H */
