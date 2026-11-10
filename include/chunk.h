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
t_chunk     get_chunk_window(const t_chunk *range);
void        release_chunk(const t_chunk *chunk);
