#ifndef MEMINFO_H
# define MEMINFO_H


enum e_meminfo_param {
    MEMAVAILABLE
};

long    meminfo(enum e_meminfo_param info);


#endif /* ! MEMINFO_H */
