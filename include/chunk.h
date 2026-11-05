#pragma once

#include <stdbool.h>
#include "file.h"


typedef struct  chunk
{
    char        *ptr;
    char        *endptr;
}               t_chunk;

/* source file: chunk.c */
int         count_chunks(void);
bool        get_next_chunk(t_chunk *chunk, struct file *file);
void        cleanout_chunk(void *arg);
t_chunk     get_chunk_window(const t_chunk *range);
void        release_chunk(const t_chunk *chunk);
