#ifndef MEMSTATE_H
# define MEMSTATE_H


struct              memstate
{
    int             page_size;
    long long       mem_available;
    long long       mem_budget;
};


void        init_memstate(struct memstate *m);
void        update_memstate(struct memstate *m);


#endif /* !MEMSTATE_H */
