#ifndef OPTPARSE_H
#define OPTPARSE_H

typedef void (*t_setopt_fn)(const char*);

struct              optmap
{
    char            id;
    t_setopt_fn     setopt;
};

/* source file: optparse.c */
void                optparse(int argc, char **argv, int *idx);

#endif /* !OPTPARSE_H */
