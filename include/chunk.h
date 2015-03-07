#pragma once

#include <stdbool.h>
#include "file.h"


typedef struct  chunk
{
    char        *ptr;
    char        *endptr;
}               t_chunk;

/* source file: chunk.c */
bool        get_next_chunk(t_chunk *chunk, struct file *file);
void        cleanout_chunk(t_chunk *chunk);
