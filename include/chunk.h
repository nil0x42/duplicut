#pragma once

#include <stdbool.h>
#include "file.h"


typedef struct  chunk
{
    char        *ptr;
    char        *endptr;
    int         skip_first_line;
}               t_chunk;

/* source file: chunk.c */
int         count_chunks(void);
bool        get_next_chunk(t_chunk *chunk, struct file *file);
void        cleanout_chunk(t_chunk *chunk);
